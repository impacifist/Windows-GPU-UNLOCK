/* Modified 2026-10-19/20 for Windows GPU UNLOCK; changes documented in CHANGES.md. */ // SPDX-License-Identifier: GPL-2.0-only
/* Modified 2026-10-19 for UEFI: target selection, checked IO, execution/quiescence hooks, diagnostics and explicit entry-latched SS1 preservation. See THIRD_PARTY_NOTICES.md.
 * Pre-driver V67 unlock, run before the stock driver binds: FWSEC-FRTS from
 * the board ROM, Booter with the V67 signature, the SS0/SS1 selector writes, then
 * FLR so the stock driver starts from a clean, halted GSP.
 */
#include <linux/aperture.h>
#include <linux/delay.h>
#include <linux/dma-mapping.h>
#include <linux/firmware.h>
#include <linux/io.h>
#include <linux/module.h>
#include <linux/pci.h>
#include <linux/slab.h>
#include <linux/timekeeping.h>

#include "diagnostic.h"
#include "gfw_eval.h"
#include "prep_610.h"
#include "unlock_610.h"
#include "vbios_610.h"

#ifdef GU_UEFI_PORT
#include "../port.h"
#endif
#ifndef GU_TARGET_PCI_ID
#define GU_TARGET_PCI_ID GU_BOARD_ID
#endif

#define SEC2_BASE 0x00840000u
#define SEC2_FBIF 0x00840600u
#define SEC2_FALCON2 0x00841000u
/* SS0 holds eight 4-bit fields; SS1 stores only its low 4 bits.
 * Historical profiles write both to 8; the explicit entry-latched mode
 * writes only SS0 and requires SS1 to retain its validated entry value. */
#define GU_SS0_UNLOCK 0x88888888u
#define GU_SS1_UNLOCK 0x8u
#define BOOTER_MAGIC 0x314c5442u
#define BOOTER_FILE "gpu-unlock/610.43.03/booter_load_ga102.bin"
#define V67_FILE "gpu-unlock/610.43.03/v67-ga102-610.43.03.bin"
#define GSP_FILE "gpu-unlock/610.43.03/gsp_ga10x.bin"
#define BOOT_FILE "gpu-unlock/610.43.03/gsp_boot_ga102.bin"
#define BOOT_DESC_FILE "gpu-unlock/610.43.03/gsp_boot_desc_ga102.bin"

struct unlock_transaction_page {
	void *cpu;
	dma_addr_t dma;
};

struct unlock_transaction_work {
	struct pci_dev *pdev;
	void __iomem *bar;
	resource_size_t bar_len;
	u16 original_cmd;
	bool cmd_valid;
	bool enabled;
	bool region;
	bool mapped;
	bool master;
	const struct firmware *booter_fw;
	const struct firmware *v67_fw;
	const struct firmware *gsp_fw;	/* borrowed from gpu_unlock_610_shared */
	const struct firmware *boot_fw;
	struct gpu_unlock_span image_span, sig_span;
	struct gpu_unlock_layout_in layout_in;
	struct gpu_unlock_layout_out layout;
	struct gpu_unlock_boot_desc boot_desc;
	struct gpu_unlock_radix_plan radix;
	struct unlock_transaction_page *pages;
	u32 page_count, pages_allocated;
	void *boot_cpu;
	dma_addr_t boot_dma;
	void *booter_cpu;
	dma_addr_t booter_dma;
	u32 booter_image_size;
	struct gpu_unlock_booter_hdr booter_hdr;
	void *v67_cpu;
	dma_addr_t v67_dma;
	void *stock_cpu;
	dma_addr_t stock_dma;
	u64 stock_size;
	void *meta_cpu;
	dma_addr_t meta_dma;
	u64 *streaming_ptr;
	u64 streaming_before;
	u64 coherent_before;
	bool streaming_set;
	bool coherent_set;
	unsigned int signature_calls;
	bool published;
	bool masks_saved;
	bool firmware_started;
	bool quiesced;
	bool retained;
	u32 plm_entry;
#ifdef GU_ENTRY_LATCHED
	u32 ss0_entry, ss1_entry;
	bool selectors_saved;
#endif
	u32 bars[6];
	void *fwsec_cpu;
	dma_addr_t fwsec_dma;
	u32 fwsec_size;
	u32 last_offset, last_value;
	bool last_valid;
	u32 last_pci_offset, last_pci_value;
	int last_pci_error;
	bool last_pci_cap;
};

static int unlock_transaction_result(struct unlock_transaction_work *work, int error,
			  const char *function, unsigned int line)
{
	if (error)
	{
		dev_err(&work->pdev->dev,
			"failure unlock_610.c:%u %s error=%d last_mmio=%#x value=%#x valid=%u mapped=%u master=%u firmware_started=%u quiesced=%u pages=%u/%u\n",
			line, function, error, work->last_offset, work->last_value,
			work->last_valid, work->mapped, work->master, work->firmware_started,
			work->quiesced, work->pages_allocated, work->page_count);
		dev_err(&work->pdev->dev, "last PCI sample: capability=%u offset=%#x value=%#x result=%d\n",
			work->last_pci_cap, work->last_pci_offset, work->last_pci_value, work->last_pci_error);
	}
	return error;
}
#define GU_RESULT(result) unlock_transaction_result(work, (result), __func__, __LINE__)

static int diag_pci_word(struct unlock_transaction_work *work, bool cap, int offset, u16 *value)
{
	int ret;
	*value = 0xffff;
	ret = cap ? pcie_capability_read_word(work->pdev, offset, value) :
		pci_read_config_word(work->pdev, offset, value);
	work->last_pci_cap = cap;
	work->last_pci_offset = offset;
	work->last_pci_value = *value;
	work->last_pci_error = ret;
	return ret;
}

static int diag_pci_dword(struct unlock_transaction_work *work, bool cap, int offset, u32 *value)
{
	int ret;
	*value = ~0u;
	ret = cap ? pcie_capability_read_dword(work->pdev, offset, value) :
		pci_read_config_dword(work->pdev, offset, value);
	work->last_pci_cap = cap;
	work->last_pci_offset = offset;
	work->last_pci_value = *value;
	work->last_pci_error = ret;
	return ret;
}

static void *diag_dma_alloc(struct unlock_transaction_work *work, const char *name,
			   size_t size, dma_addr_t *dma, unsigned int flags)
{
	void *memory = dma_alloc_coherent(&work->pdev->dev, size, dma, flags);
	if (!memory)
		dev_err(&work->pdev->dev, "DMA allocation failed: asset=%s bytes=%zu pages_allocated=%u requested=%u\n",
			name, size, work->pages_allocated, work->page_count);
	return memory;
}

/* CONFIG_FW_CACHE attaches name records to devres even though this is not
 * devm_request_firmware(). release_firmware() does not remove them, and
 * really_probe() refuses a device with any existing devres. Scope each
 * synchronous request so only its cache records are released; the returned
 * firmware remains owned by the caller.
 */
static int request_scoped(struct device *dev, const struct firmware **fw, const char *name)
{
	void *group;
	int ret, released;

	group = devres_open_group(dev, NULL, GFP_KERNEL);
	if (!group)
		return -ENOMEM;
	dev_info(dev, "asset request: %s\n", name);
	ret = request_firmware(fw, name, dev);
	released = devres_release_group(dev, group);
	dev_info(dev, "asset result: %s error=%d bytes=%zu\n",
		name, ret, ret ? (size_t)0 : (*fw)->size);
	dev_info(dev, "asset cache cleanup: %s devres_released=%d\n", name, released);
	return ret;
}

static int request_asset(struct unlock_transaction_work *work, const struct firmware **fw,
			 const char *name)
{
	return GU_RESULT(request_scoped(&work->pdev->dev, fw, name));
}

static int check_devres_empty(struct unlock_transaction_work *work, const char *phase)
{
	struct device *dev = &work->pdev->dev;
	unsigned long flags;
	bool empty;

	/* The caller also holds the device lock across the whole transaction. */
	spin_lock_irqsave(&dev->devres_lock, flags);
	empty = list_empty(&dev->devres_head);
	spin_unlock_irqrestore(&dev->devres_lock, flags);
	dev_info(dev, "driver probe resources: phase=%s empty=%u\n", phase, empty);
	return GU_RESULT(empty ? 0 : -EBUSY);
}

static int read_bar(struct unlock_transaction_work *work, u32 offset, u32 *value)
{
#ifdef GU_UEFI_PORT
	if (Gu.fault)
		return GU_RESULT(-EIO);
#endif
	work->last_offset = offset;
	work->last_valid = false;
	if (!work->mapped || offset > work->bar_len || work->bar_len - offset < 4)
		return GU_RESULT(-EINVAL);
	*value = readl(work->bar + offset);
#ifdef GU_UEFI_PORT
	if (Gu.fault)
		return GU_RESULT(-EIO);
#endif
	work->last_value = *value;
	work->last_valid = true;
	return 0;
}

static void write_bar(struct unlock_transaction_work *work, u32 offset, u32 value)
{
	writel(value, work->bar + offset);
}

static int poll_us(struct unlock_transaction_work *work, u32 offset, u32 mask, u32 expect,
		   unsigned int budget_us, bool timeout_is_failure)
{
	ktime_t deadline = ktime_add_us(ktime_get(), budget_us);
	u32 value = 0;

	for (;;) {
		if (read_bar(work, offset, &value) || value == ~0u ||
		    (value & 0xffff0000u) == 0xbadf0000u) {
			dev_err(&work->pdev->dev, "poll inaccessible: reg=%#x last=%#x mask=%#x expected=%#x budget_us=%u\n",
				offset, value, mask, expect, budget_us);
			return GU_RESULT(-EIO);
		}
		if ((value & mask) == expect)
			return 0;
		if (ktime_compare(ktime_get(), deadline) >= 0) {
			/* Console loglevel hides info. A tolerated timeout must not
			 * look like the failure that stopped the boot. */
			if (!timeout_is_failure) {
				dev_info(&work->pdev->dev, "poll timeout tolerated: reg=%#x last=%#x mask=%#x expected=%#x budget_us=%u\n",
					offset, value, mask, expect, budget_us);
				return -ETIMEDOUT;
			}
			dev_err(&work->pdev->dev, "poll timeout: reg=%#x last=%#x mask=%#x expected=%#x budget_us=%u\n",
				offset, value, mask, expect, budget_us);
			return GU_RESULT(-ETIMEDOUT);
		}
		usleep_range(50, 100);
	}
}

struct falcon_eng {
	u32 base;
	u32 fbif;
	u32 falcon2;
};

static bool inaccessible(u32 value)
{
	return value == ~0u || (value & 0xffff0000u) == 0xbadf0000u;
}

#ifdef GU_ENTRY_LATCHED
/* Read-only bridge to the app's private, immutable latch. Never learn here.
 * Use only before the intended SS0 write, including Falcon reset/DMA. The
 * post-FLR hold_sec2_reset path instead checks SS0 against GU_SS0_UNLOCK. */
static int check_entry_selectors(struct unlock_transaction_work *work)
{
	u32 ss0, ss1;

	if (!work->selectors_saved || !Gu.profile_verified ||
	    read_bar(work, GU_FEAT_SS0_OFFSET, &ss0) ||
	    read_bar(work, GU_FEAT_SS1_OFFSET, &ss1) ||
	    !GuEntrySelectorsMatch(ss0, ss1) ||
	    ss0 != work->ss0_entry || ss1 != work->ss1_entry)
		return GU_RESULT(-EIO);
	return 0;
}
#endif
#ifdef GU_ENTRY_LATCHED_SS1_PRESERVE
#define GU_SS1_EXPECTED(work) ((work)->ss1_entry)
#else
#define GU_SS1_EXPECTED(work) GU_SS1_UNLOCK
#endif

static int falcon_reset(struct unlock_transaction_work *work, const struct falcon_eng *eng)
{
	u32 engine, hwcfg, bcr, chip;
	unsigned int i;
	int ret;

	/* 610 kflcnPreResetWait_GA102 permits this timeout (HW bug 3419321). */
	dev_info(&work->pdev->dev, "Falcon reset begin: base=%#x\n", eng->base);
	ret = poll_us(work, eng->base + 0xf4, 1u << 31, 1u << 31, 1000, false);
	if (ret && ret != -ETIMEDOUT)
		return GU_RESULT(ret);
	if (ret)
		dev_info(&work->pdev->dev, "Falcon pre-reset timeout tolerated per HW bug 3419321\n");
	if (read_bar(work, eng->base + 0x3c0, &engine) || inaccessible(engine))
		return GU_RESULT(-ENODEV);
#ifdef GU_ENTRY_LATCHED
	/* After every pre-reset log, at the first hardware-change boundary. */
	ret = check_entry_selectors(work);
	if (ret)
		return ret;
#endif
	write_bar(work, eng->base + 0x3c0, engine | 1);
	for (i = 0; i < 10; i++)
		readl(work->bar + eng->base + 0x3c0);
	write_bar(work, eng->base + 0x3c0, engine & ~1u);
	for (i = 0; i < 10; i++)
		readl(work->bar + eng->base + 0x3c0);
	ret = poll_us(work, eng->base + 0xf4, 1u << 12, 0, 1000000, true);
	if (ret)
		return GU_RESULT(ret);
	if (read_bar(work, eng->base + 0xf4, &hwcfg) || inaccessible(hwcfg))
		return GU_RESULT(-EIO);
	if (hwcfg & (1u << 10)) {
		if (read_bar(work, eng->falcon2 + 0x668, &bcr) || inaccessible(bcr))
			return GU_RESULT(-ENODEV);
		if (bcr & (1u << 4)) {
			write_bar(work, eng->falcon2 + 0x668, 0);
			ret = poll_us(work, eng->falcon2 + 0x668, 0x11, 1, 1000000, true);
			if (ret)
				return GU_RESULT(ret);
		}
	}
	if (read_bar(work, 0, &chip) || inaccessible(chip))
		return GU_RESULT(-ENODEV);
	write_bar(work, eng->base + 0x84, chip);
	dev_info(&work->pdev->dev, "Falcon reset done: base=%#x hwcfg=%#x chip=%#x\n", eng->base, hwcfg, chip);
	return 0;
}

/* DMATRFMOFFS is 24 bits, FBOFFS is 32 bits, and BASE is 49 bits.
 * Reject truncation/wrap before submitting any part of a transfer. */
static bool falcon_dma_valid(u32 dest, u32 mem_off, dma_addr_t src, u32 size)
{
	return size && !((size | dest | mem_off | src) & 255) &&
		(u64)dest + size <= (1ull << 24) &&
		(u64)mem_off + size <= (1ull << 32) &&
		src < (1ull << 49) &&
		(u64)mem_off + size <= (1ull << 49) - src;
}

static int falcon_dma(struct unlock_transaction_work *work, u32 base, u32 dest, u32 mem_off,
		      dma_addr_t src, u32 size, u32 cmd)
{
	u32 done = 0;
	int ret;

	dev_info(&work->pdev->dev, "Falcon DMA: base=%#x dest=%#x offset=%#x src=%#llx size=%#x cmd=%#x\n",
		base, dest, mem_off, (unsigned long long)src, size, cmd);
	if (!falcon_dma_valid(dest, mem_off, src, size))
		return GU_RESULT(-EINVAL);
	ret = poll_us(work, base + 0x118, 1, 0, 1000000, true);
	if (ret)
		return GU_RESULT(ret);
#ifdef GU_ENTRY_LATCHED
	/* Recheck after the external log, before publishing a DMA source. */
	ret = check_entry_selectors(work);
	if (ret)
		return ret;
#endif
	write_bar(work, base + 0x110, lower_32_bits(src >> 8));
	write_bar(work, base + 0x128, upper_32_bits(src >> 8) & 0x1ff);
	while (done < size) {
		ret = poll_us(work, base + 0x118, 1, 0, 1000000, true);
		if (ret) {
			dev_err(&work->pdev->dev, "Falcon DMA progress: done=%u total=%u\n", done, size);
			return GU_RESULT(ret);
		}
		write_bar(work, base + 0x114, dest);
		write_bar(work, base + 0x11c, mem_off);
		write_bar(work, base + 0x118, cmd);
		done += 256;
		dest += 256;
		mem_off += 256;
	}
	return GU_RESULT(poll_us(work, base + 0x118, 2, 2, 1000000, true));
}

#ifdef GU_UEFI_PORT
/* GA102 registers published in NVIDIA 610.43.03. This reads status, not
 * authentication internals. Protected register values are logged verbatim;
 * neither a readable snapshot nor HALTED proves successful FWSEC execution. */
static int fwsec_snapshot(struct unlock_transaction_work *work, const char *phase)
{
	static const u32 offsets[] = {
		0x110100, 0x110008, 0x110040, 0x110044, 0x110094, 0x1100f4,
		0x110104, 0x11010c, 0x110118, 0x110600, 0x110624, 0x111668,
		0x111180, 0x111198, 0x11119c, 0x111210,
		0x110108, 0x110110, 0x110114, 0x11011c, 0x110128,
	};
	u32 values[ARRAY_SIZE(offsets)], count = 0, i;
	int ret = 0;

	/* Read first, then log so USB/console latency does not split the sample. */
	for (i = 0; i < ARRAY_SIZE(offsets); i++) {
		ret = read_bar(work, offsets[i], &values[i]);
		if (ret)
			break;
		count++;
	}
	dev_info(&work->pdev->dev, "FWSEC snapshot: phase=%s samples=%u/%u error=%d\n",
		 phase, count, (unsigned int)ARRAY_SIZE(offsets), ret);
	for (i = 0; i < count; i++)
		dev_info(&work->pdev->dev, "FWSEC register: reg=%#x value=%#x sentinel_like=%u\n",
			 offsets[i], values[i], inaccessible(values[i]));
	return ret ? ret : (Gu.fault ? -EIO : 0);
}

static int fwsec_wait_halt(struct unlock_transaction_work *work)
{
	ktime_t deadline = ktime_add_us(ktime_get(), 5000000);
	u32 value = 0, first = 0, samples = 0, requested_wait_us = 0;
	bool observed_nonhalt = false;
	int ret;

	for (;;) {
		ret = read_bar(work, 0x110100, &value);
		if (ret)
			break;
		if (!samples)
			first = value;
		samples++;
		if (inaccessible(value)) { ret = -EIO; break; }
		if (value & 0x10)
			break;
		observed_nonhalt = true;
		if (ktime_compare(ktime_get(), deadline) >= 0) { ret = -ETIMEDOUT; break; }
		usleep_range(50, 100);
		requested_wait_us += 50;
	}
	/* Gu.ticks counts requested Stall intervals, NOT elapsed wall-clock time. */
	dev_info(&work->pdev->dev, "FWSEC halt observation: first=%#x last=%#x samples=%u observed_nonhalt=%u requested_wait_us=%u error=%d\n",
		first, value, samples, observed_nonhalt, requested_wait_us, ret);
	return GU_RESULT(ret ? ret : (Gu.fault ? -EIO : 0));
}
#endif

static int falcon_hs(struct unlock_transaction_work *work, const struct falcon_eng *eng,
		     u32 code_off, u32 code_size, u32 code_va, u32 code_pa,
		     u32 data_off, u32 data_size, u32 data_pa,
		     u32 sig_dmem, u32 engine_mask, u32 ucode_id,
		     dma_addr_t image, u32 mailbox0, u32 mailbox1,
		     bool wait_plm)
{
	u32 trans, cpuctl;
	dma_addr_t code_src;
	int ret;

	if (image > ~0ull - code_off || image + code_off < code_va ||
	    image > ~0ull - data_off)
		return GU_RESULT(-ERANGE);
	code_src = image + code_off - code_va;
	if (!falcon_dma_valid(code_pa, code_va, code_src, code_size) ||
	    (data_size && !falcon_dma_valid(data_pa, 0, image + data_off, data_size)))
		return GU_RESULT(-EINVAL);
	ret = falcon_reset(work, eng);
	if (ret)
		return GU_RESULT(ret);
	if (read_bar(work, eng->fbif + 0x24, &trans) || inaccessible(trans))
		return GU_RESULT(-EIO);
	trans |= 1u << 7;
	write_bar(work, eng->fbif + 0x24, trans);
	write_bar(work, eng->base + 0x10c, 0);
	if (read_bar(work, eng->fbif, &trans) || inaccessible(trans))
		return GU_RESULT(-ENODEV);
	trans = (trans & ~7u) | 5u;
	write_bar(work, eng->fbif, trans);
	dma_wmb();
	ret = falcon_dma(work, eng->base, code_pa, code_va, code_src,
			 code_size, gpu_unlock_dma_cmd(true));
	if (ret)
		return GU_RESULT(ret);
	if (data_size) {
		ret = falcon_dma(work, eng->base, data_pa, 0, image + data_off, data_size,
				 gpu_unlock_dma_cmd(false));
		if (ret)
			return GU_RESULT(ret);
	}
	/* Follow the reference DMA -> PKC -> START path. DMEM PIO visibility
	 * before START is not established on this board; do not touch its PIO
	 * window or treat it as proof of staging. Scratch/WPR checks follow. */
	write_bar(work, eng->falcon2 + 0x210, sig_dmem);
	write_bar(work, eng->falcon2 + 0x19c, engine_mask);
	write_bar(work, eng->falcon2 + 0x198, ucode_id & 0xff);
	write_bar(work, eng->falcon2 + 0x180, 1);
	write_bar(work, eng->base + 0x104, code_va);
	write_bar(work, eng->base + 0x40, mailbox0);
	write_bar(work, eng->base + 0x44, mailbox1);
#ifdef GU_UEFI_PORT
	if (!wait_plm && eng->base == 0x110000) {
		ret = fwsec_snapshot(work, "pre-start");
		if (ret)
			return GU_RESULT(ret);
	}
#endif
	if (read_bar(work, eng->base + 0x100, &cpuctl) || inaccessible(cpuctl))
		return GU_RESULT(-EIO);
	dev_info(&work->pdev->dev, "Falcon start: base=%#x cpuctl=%#x code_va=%#x sig_dmem=%#x engine_mask=%#x ucode=%u mailbox=%#x:%#x\n",
		eng->base, cpuctl, code_va, sig_dmem, engine_mask, ucode_id, mailbox1, mailbox0);
	if (cpuctl & (1u << 6))
		write_bar(work, eng->base + 0x130, 1u << 1);
	else
		write_bar(work, eng->base + 0x100, 1u << 1);
	if (wait_plm) {
		ktime_t deadline = ktime_add_us(ktime_get(), 5000000);

		for (;;) {
			u32 plm, engine, identity;

			if (diag_pci_dword(work, false, PCI_VENDOR_ID, &identity) ||
			    identity != GU_TARGET_PCI_ID ||
			    read_bar(work, eng->base + 0x100, &engine) || engine == ~0u ||
			    (engine & 0xffff0000u) == 0xbadf0000u ||
			    read_bar(work, GU_FEAT_PLM_OFFSET, &plm))
				return GU_RESULT(-ENODEV);
			/* Cold entry was closed: this transition proves payload execution. */
			if (plm == ~0u && work->plm_entry != ~0u)
				return 0;
			if ((engine & (1u << 4)) || ktime_compare(ktime_get(), deadline) >= 0)
				dev_err(&work->pdev->dev, "early PLM failed: pci=%#x cpuctl=%#x plm=%#x entry=%#x budget_us=5000000\n",
					identity, engine, plm, work->plm_entry);
			if (engine & (1u << 4))
				return GU_RESULT(-EIO);
			if (ktime_compare(ktime_get(), deadline) >= 0)
				return GU_RESULT(-ETIMEDOUT);
			usleep_range(50, 100);
		}
	}
#ifdef GU_UEFI_PORT
	if (eng->base == 0x110000) {
		int snapshot_ret;
		ret = fwsec_wait_halt(work);
		snapshot_ret = fwsec_snapshot(work, ret ? "halt-error" : "post-halt");
		if (!ret)
			ret = snapshot_ret;
	} else
#endif
		ret = poll_us(work, eng->base + 0x100, 1u << 4, 1u << 4, 5000000, true);
	if (ret)
		return GU_RESULT(ret);
	return 0;
}

static int load_booter(struct unlock_transaction_work *work)
{
	const u8 *data;
	u32 magic, version, image_size, sig_bytes;
	struct gpu_unlock_booter_hdr hdr;
	int ret;

	ret = request_asset(work, &work->booter_fw, BOOTER_FILE);
	if (ret)
		return GU_RESULT(ret);
	data = work->booter_fw->data;
	if (work->booter_fw->size < 72)
		return GU_RESULT(-EINVAL);
	memcpy(&magic, data, sizeof(magic));
	memcpy(&version, data + 4, sizeof(version));
	if (magic != BOOTER_MAGIC || version != 1)
		return GU_RESULT(-EINVAL);
	memcpy(&hdr, data + 8, sizeof(hdr));
	memcpy(&image_size, data + 8 + sizeof(hdr), sizeof(image_size));
	memcpy(&sig_bytes, data + 12 + sizeof(hdr), sizeof(sig_bytes));
	if (!hdr.ucode_id || hdr.num_apps != 1 || sig_bytes != hdr.sig_bytes ||
	    image_size != GU_BOOTER_IMAGE_SIZE ||
	    work->booter_fw->size != 72u + image_size + sig_bytes)
		return GU_RESULT(-EINVAL);
	/* Only this pinned production BOOT_FROM_HS geometry is implemented. */
	if (hdr.ucode_id != 3 || hdr.engine_id != 1 || hdr.num_sigs != 2 ||
	    sig_bytes != GU_BOOTER_SIG_TOTAL || hdr.os_code_off != 0 ||
	    hdr.os_code_size != 256 || hdr.os_data_off != 35328 ||
	    hdr.os_data_size != 25088 || hdr.app_code_off != 256 ||
	    hdr.app_code_size != 35072 || hdr.app_data_off != 256 ||
	    hdr.app_data_size != 0 || hdr.patch_loc != 35344)
		return GU_RESULT(-EINVAL);
	work->booter_hdr = hdr;
	work->booter_image_size = image_size;
	/* Allocation follows the ownership and DMA-mask snapshot, not asset lookup. */
	return 0;
}

static int patch_booter(struct unlock_transaction_work *work)
{
	const u8 *data = work->booter_fw->data;
	u32 fuse_raw, fuse_ver;
	u32 index = work->booter_hdr.ucode_id - 1;

	if (read_bar(work, GU_FUSE_SEC2_UCODE1 + index * 4, &fuse_raw))
		return GU_RESULT(-EIO);
	fuse_ver = gpu_unlock_fuse_version(fuse_raw);
	dev_info(&work->pdev->dev, "Booter signature: fuse_raw=%#x version=%u signatures=%u size=%u\n",
		fuse_raw, fuse_ver, work->booter_hdr.num_sigs, work->booter_hdr.sig_bytes);
	return GU_RESULT(gpu_unlock_booter_patch(work->booter_cpu, work->booter_image_size,
				       &work->booter_hdr,
				       data + 72 + work->booter_image_size,
				       work->booter_hdr.sig_bytes, fuse_ver));
}

static void free_coherent(struct unlock_transaction_work *work)
{
	u32 i;

	for (i = 0; i < work->pages_allocated; i++)
		dma_free_coherent(&work->pdev->dev, GU_RADIX_PAGE,
				  work->pages[i].cpu, work->pages[i].dma);
	kvfree(work->pages);
	work->pages = NULL;
	work->pages_allocated = 0;
	if (work->boot_cpu)
		dma_free_coherent(&work->pdev->dev, work->boot_desc.image_size,
				  work->boot_cpu, work->boot_dma);
	work->boot_cpu = NULL;
	if (work->fwsec_cpu) {
		dma_free_coherent(&work->pdev->dev, work->fwsec_size,
				  work->fwsec_cpu, work->fwsec_dma);
		work->fwsec_cpu = NULL;
	}
	if (work->booter_cpu)
		dma_free_coherent(&work->pdev->dev, GU_BOOTER_IMAGE_SIZE,
				  work->booter_cpu, work->booter_dma);
	if (work->v67_cpu)
		dma_free_coherent(&work->pdev->dev, GU_V67_SIZE, work->v67_cpu,
				  work->v67_dma);
	if (work->stock_cpu)
		dma_free_coherent(&work->pdev->dev, work->stock_size,
				  work->stock_cpu, work->stock_dma);
	if (work->meta_cpu)
		dma_free_coherent(&work->pdev->dev, GU_WPR_META_SIZE,
				  work->meta_cpu, work->meta_dma);
	work->booter_cpu = NULL;
	work->v67_cpu = NULL;
	work->stock_cpu = NULL;
	work->meta_cpu = NULL;
}

static int unlock_transaction_unwind(struct unlock_transaction_work *work)
{
	u16 now, want;
	int ret = 0;

	/* A timeout never authorizes freeing firmware-visible memory. */
	if (work->firmware_started && !work->quiesced) {
		work->retained = true;
		return GU_RESULT(-EBUSY);
	}
	if (work->master) {
		pci_clear_master(work->pdev);
		work->master = false;
	}
	if (work->mapped) {
		pci_iounmap(work->pdev, work->bar);
		work->bar = NULL;
		work->mapped = false;
	}
	if (work->region) {
		pci_release_region(work->pdev, 0);
		work->region = false;
	}
	if (work->enabled) {
		pci_disable_device(work->pdev);
		work->enabled = false;
	}
	free_coherent(work);
	if (work->booter_fw) {
		release_firmware(work->booter_fw);
		work->booter_fw = NULL;
	}
	if (work->v67_fw) {
		release_firmware(work->v67_fw);
		work->v67_fw = NULL;
	}
	if (work->boot_fw) {
		release_firmware(work->boot_fw);
		work->boot_fw = NULL;
	}
	/* Match the driver core's entry condition, not just BAR/mask cleanup.
	 * Never release unrelated resources to make this check pass.
	 */
	if (check_devres_empty(work, "handoff"))
		ret = -EBUSY;
	if (work->streaming_set && work->streaming_ptr) {
		if (work->pdev->dev.dma_mask != work->streaming_ptr)
			return GU_RESULT(-EIO);
		if (dma_set_mask(&work->pdev->dev, work->streaming_before) ||
		    *work->streaming_ptr != work->streaming_before)
			ret = -EIO;
		dev_info(&work->pdev->dev, "restore streaming mask: actual=%#llx expected=%#llx\n",
			(unsigned long long)*work->streaming_ptr, (unsigned long long)work->streaming_before);
	}
	if (work->coherent_set) {
		if (dma_set_coherent_mask(&work->pdev->dev, work->coherent_before) ||
		    work->pdev->dev.coherent_dma_mask != work->coherent_before)
			ret = -EIO;
		dev_info(&work->pdev->dev, "restore coherent mask: actual=%#llx expected=%#llx\n",
			(unsigned long long)work->pdev->dev.coherent_dma_mask,
			(unsigned long long)work->coherent_before);
	}
	if (!work->cmd_valid)
		return GU_RESULT(ret);
	if (diag_pci_word(work, false, PCI_COMMAND, &now) || now == 0xffff)
		return GU_RESULT(-ENODEV);
	want = gpu_unlock_command_restore(work->original_cmd, now);
	if (want != now && pci_write_config_word(work->pdev, PCI_COMMAND, want))
		return GU_RESULT(-EIO);
	if (diag_pci_word(work, false, PCI_COMMAND, &now))
		return GU_RESULT(-EIO);
	dev_info(&work->pdev->dev, "restore PCI command: actual=%#x original=%#x mask=%#x\n",
		now, work->original_cmd, GU_PCI_COMMAND_MEMORY | GU_PCI_COMMAND_MASTER);
	if ((now & (GU_PCI_COMMAND_MEMORY | GU_PCI_COMMAND_MASTER)) !=
	    (work->original_cmd & (GU_PCI_COMMAND_MEMORY | GU_PCI_COMMAND_MASTER)))
		return GU_RESULT(-EIO);
	return GU_RESULT(ret);
}

static int unlock_transaction_assets(struct unlock_transaction_work *work)
{
	const struct firmware *desc;
	int ret;

	ret = check_devres_empty(work, "entry");
	if (ret)
		return ret;
	ret = request_asset(work, &work->v67_fw, V67_FILE);
	if (ret)
		return GU_RESULT(ret);
	if (work->v67_fw->size != GU_V67_SIZE)
		return GU_RESULT(-EBADMSG);
	ret = gpu_unlock_elf_section(work->gsp_fw->data, work->gsp_fw->size,
				    ".fwimage", &work->image_span);
	if (ret)
		return GU_RESULT(ret);
	ret = gpu_unlock_elf_section(work->gsp_fw->data, work->gsp_fw->size,
				    ".fwsignature_ga10x", &work->sig_span);
	if (ret)
		return GU_RESULT(ret);
	if (!work->image_span.size || work->image_span.size > 128ull << 20 ||
	    work->sig_span.size != 4096)
		return GU_RESULT(-EINVAL);
	ret = gpu_unlock_radix_plan(work->image_span.size, &work->radix);
	if (ret)
		return GU_RESULT(ret);
	ret = request_asset(work, &work->boot_fw, BOOT_FILE);
	if (ret)
		return GU_RESULT(ret);
	if (work->boot_fw->size != 24576)
		return GU_RESULT(-EINVAL);
	ret = request_asset(work, &desc, BOOT_DESC_FILE);
	if (ret)
		return GU_RESULT(ret);
	ret = gpu_unlock_boot_desc_parse(desc->data, desc->size,
					work->boot_fw->size, &work->boot_desc);
	dev_info(&work->pdev->dev, "asset geometry: image=%llu signature=%llu boot=%u code=%#x data=%#x manifest=%#x parse=%d\n",
		(unsigned long long)work->image_span.size, (unsigned long long)work->sig_span.size,
		work->boot_desc.image_size, work->boot_desc.code_offset, work->boot_desc.data_offset,
		work->boot_desc.manifest_offset, ret);
	release_firmware(desc);
	return GU_RESULT(ret ? ret : load_booter(work));
}

static int unlock_transaction_layout(struct unlock_transaction_work *work)
{
	struct gpu_unlock_layout_in *in = &work->layout_in;
	u32 mb, vga, lo, hi, plm;
	int ret;

	if (read_bar(work, 0x001183a4, &mb) || !mb || mb == ~0u ||
	    read_bar(work, 0x00625f04, &vga) || vga == ~0u ||
	    read_bar(work, 0x001fa7c8, &plm) || plm == ~0u || !(plm & 1) ||
	    read_bar(work, 0x001fa82c, &lo) || lo == ~0u ||
	    read_bar(work, 0x001fa830, &hi) || hi == ~0u)
		return GU_RESULT(-ENODEV);
	/* Keep the cold board inputs in the USB kernel log even on layout failure.
	 * Register values after NVIDIA binds cannot reconstruct this state. */
	dev_info(&work->pdev->dev,
		 "layout inputs: fb_mb=%#x vga=%#x mmu_plm=%#x mmu_lo=%#x mmu_hi=%#x image=%llu boot=%u\n",
		 mb, vga, plm, lo, hi, (unsigned long long)work->image_span.size,
		 work->boot_desc.image_size);
	in->usable_fb = (u64)mb << 20;
	if (in->usable_fb < 256ull << 20) {
		dev_err(&work->pdev->dev, "layout rejected: usable FB below 256 MiB\n");
		return GU_RESULT(-ERANGE);
	}
	/* kgspPopulateWprMeta_TU102: PRAMIN is the top 1 MiB fallback. */
	in->vga_base = in->usable_fb - (1ull << 20);
	if (vga & (1u << 3)) {
		in->vga_base = (u64)(vga >> 8) << 16;
		if (in->vga_base < in->usable_fb - (1ull << 20))
			in->vga_base = in->usable_fb - 0x20000;
	}
	if (in->vga_base >= in->usable_fb) {
		dev_err(&work->pdev->dev, "layout rejected: VGA base at or above usable FB\n");
		return GU_RESULT(-ERANGE);
	}
	in->mmu_lock_low = (u64)(lo >> 4) << 12;
	{
		/* VAL is bits 31:4; the address is that field shifted by 12.
		 * memmgrBlockMemLockedMemory_GA100 only treats the lock as the
		 * VBIOS end-of-FB reservation when its exclusive high address is
		 * the FB top. A lock elsewhere is not that reservation. GA102
		 * has no scrubber, so WPR must stay in the pre-scrubbed top. */
		u64 mmu_hi = (u64)(hi >> 4) << 12;
		u64 mmu_top = mmu_hi > ~0ull - 0xffffull ? ~0ull :
			(mmu_hi + 0xffffull) & ~0xffffull;
		bool top_lock;

		in->mmu_lock_valid = mmu_hi >= in->mmu_lock_low;
		top_lock = in->mmu_lock_valid && (mmu_top >> 20) == (in->usable_fb >> 20);
		dev_info(&work->pdev->dev,
			 "layout mmu: lo=%#llx hi=%#llx hi_exclusive=%#llx constrain=%u\n",
			 (unsigned long long)in->mmu_lock_low, (unsigned long long)mmu_hi,
			 (unsigned long long)mmu_top, top_lock);
		if (!top_lock) {
			dev_info(&work->pdev->dev,
				 "mmu lock ignored: high address is not the FB top\n");
			in->mmu_lock_valid = false;
		}
	}
	in->frts_size = 1ull << 20;
	in->boot_size = work->boot_desc.image_size;
	in->image_size = work->image_span.size;
	in->nonwpr_size = 1ull << 20;
	/* GA102's supported minimum heap; no GSP-RM clients are started here. */
	in->heap_request = 88ull << 20;
	ret = gpu_unlock_layout(in, &work->layout);
	dev_info(&work->pdev->dev,
		 "layout decoded: fb=%#llx vga=%#llx mmu_valid=%u mmu_low=%#llx frts=%#llx heap=%#llx result=%d\n",
		 (unsigned long long)in->usable_fb, (unsigned long long)in->vga_base,
		 in->mmu_lock_valid, (unsigned long long)in->mmu_lock_low,
		 (unsigned long long)in->frts_size, (unsigned long long)in->heap_request, ret);
	if (ret) {
		dev_err(&work->pdev->dev, "layout rejected: reservation calculation (%d)\n", ret);
		return GU_RESULT(ret);
	}
	dev_info(&work->pdev->dev,
		 "layout ranges: nonwpr=%#llx wpr=%#llx heap=%#llx gsp=%#llx boot=%#llx frts=%#llx end=%#llx scrub_floor=%#llx\n",
		 (unsigned long long)work->layout.nonwpr_start,
		 (unsigned long long)work->layout.wpr_start,
		 (unsigned long long)work->layout.heap_offset,
		 (unsigned long long)work->layout.gsp_offset,
		 (unsigned long long)work->layout.boot_offset,
		 (unsigned long long)work->layout.frts_offset,
		 (unsigned long long)work->layout.wpr_end,
		 (unsigned long long)(in->usable_fb - (256ull << 20)));
	/* GA102 has no scrubber: all reservations must fit the pre-scrubbed top. */
	if (work->layout.nonwpr_start < in->usable_fb - (256ull << 20)) {
		dev_err(&work->pdev->dev, "layout rejected: reservations below pre-scrubbed top 256 MiB\n");
		return GU_RESULT(-ERANGE);
	}
	return 0;
}

static int unlock_transaction_radix(struct unlock_transaction_work *work)
{
	const struct gpu_unlock_radix_plan *r = &work->radix;
	u64 *addresses;
	u32 i, level, first = 0;
	int ret = 0;

	work->page_count = r->table_pages + r->pages[3];
	work->pages = kvcalloc(work->page_count, sizeof(*work->pages), GFP_KERNEL);
	addresses = kvcalloc(work->page_count, sizeof(*addresses), GFP_KERNEL);
	if (!work->pages || !addresses) {
		kvfree(addresses);
		return GU_RESULT(-ENOMEM);
	}
	for (i = 0; i < work->page_count; i++) {
		struct unlock_transaction_page *page = &work->pages[i];

		page->cpu = diag_dma_alloc(work, "radix-page", GU_RADIX_PAGE,
					       &page->dma, GFP_KERNEL);
		if (!page->cpu) {
			ret = -ENOMEM;
			goto out;
		}
		work->pages_allocated++;
		addresses[i] = page->dma;
		memset(page->cpu, 0, GU_RADIX_PAGE);
		if (i >= r->table_pages) {
			u64 offset = (i - r->table_pages) * GU_RADIX_PAGE;
			size_t n = min_t(u64, GU_RADIX_PAGE, work->image_span.size - offset);

			memcpy(page->cpu, work->gsp_fw->data + work->image_span.offset + offset, n);
		}
	}
	for (level = 0; level < 3; level++) {
		u32 child = first + r->pages[level];

		for (i = 0; i < r->pages[level]; i++) {
			u32 at = i * (GU_RADIX_PAGE / 8);
			u32 n = min_t(u64, GU_RADIX_PAGE / 8, r->pages[level + 1] - at);

			ret = gpu_unlock_radix_page(work->pages[first + i].cpu,
						    addresses + child + at, n);
			if (ret)
				goto out;
		}
		first = child;
	}
out:
	kvfree(addresses);
	return GU_RESULT(ret);
}

static int unlock_transaction_claim(struct unlock_transaction_work *work)
{
	u16 command, pmcsr;
	u32 cap;
	unsigned int i;
	int ret;

	dev_info(&work->pdev->dev, "claim: power=%d pm_cap=%#x flags=%#x\n",
		work->pdev->current_state, work->pdev->pm_cap, work->pdev->dev_flags);
	if (work->pdev->current_state != PCI_D0 ||
	    (work->pdev->pm_cap &&
	     (diag_pci_word(work, false, work->pdev->pm_cap + PCI_PM_CTRL, &pmcsr) ||
	      (pmcsr & PCI_PM_CTRL_STATE_MASK) != PCI_D0)))
		return GU_RESULT(-EOPNOTSUPP);
	if ((work->pdev->dev_flags & PCI_DEV_FLAGS_NO_FLR_RESET) ||
	    !pci_is_pcie(work->pdev) ||
	    diag_pci_dword(work, true, PCI_EXP_DEVCAP, &cap) ||
	    !(cap & PCI_EXP_DEVCAP_FLR))
		return GU_RESULT(-EOPNOTSUPP);
	if (diag_pci_word(work, false, PCI_COMMAND, &command) || command == 0xffff)
		return GU_RESULT(-ENODEV);
	work->original_cmd = command;
	work->cmd_valid = true;
	if (command & PCI_COMMAND_MASTER)
		return GU_RESULT(-EBUSY);
	dev_info(&work->pdev->dev, "claim: command=%#x capability=%#x BAR_mask=%#x\n",
		command, cap, pci_select_bars(work->pdev, IORESOURCE_MEM));
	/* Only BAR0 is mapped. On the display card the firmware framebuffer keeps
	 * BAR1 until just before the FLR, as it does while the stock driver boots.
	 */
	ret = pci_request_region(work->pdev, 0, "gpu_unlock");
	if (ret)
		return GU_RESULT(ret);
	work->region = true;
	if (!(command & PCI_COMMAND_MEMORY)) {
		ret = pci_enable_device_mem(work->pdev);
		if (ret)
			return GU_RESULT(ret);
		work->enabled = true;
	}
	work->bar_len = pci_resource_len(work->pdev, 0);
	if (work->bar_len < SEC2_FALCON2 + 0x1000)
		return GU_RESULT(-EINVAL);
	for (i = 0; i < ARRAY_SIZE(work->bars); i++)
		if (diag_pci_dword(work, false, PCI_BASE_ADDRESS_0 + i * 4, &work->bars[i]))
			return GU_RESULT(-EIO);
	ret = pci_save_state(work->pdev);
	if (ret)
		return GU_RESULT(ret);
	work->bar = pci_iomap(work->pdev, 0, 0);
	if (!work->bar)
		return GU_RESULT(-ENOMEM);
	work->mapped = true;
	return 0;
}

static int unlock_transaction_dma(struct unlock_transaction_work *work)
{
	struct device *dev = &work->pdev->dev;

	if (!dev->dma_mask || work->masks_saved)
		return GU_RESULT(-EINVAL);
	work->masks_saved = true;
	work->streaming_ptr = dev->dma_mask;
	work->streaming_before = *dev->dma_mask;
	work->coherent_before = dev->coherent_dma_mask;
	dev_info(dev, "DMA masks: streaming=%#llx coherent=%#llx requested=%#llx\n",
		(unsigned long long)work->streaming_before, (unsigned long long)work->coherent_before,
		(unsigned long long)DMA_BIT_MASK(32));
	work->streaming_set = true;
	if (GU_DIAG(dma_set_mask(dev, DMA_BIT_MASK(32))))
		return GU_RESULT(-EIO);
	work->coherent_set = true;
	if (GU_DIAG(dma_set_coherent_mask(dev, DMA_BIT_MASK(32))))
		return GU_RESULT(-EIO);
	work->booter_cpu = diag_dma_alloc(work, "booter", work->booter_image_size,
					      &work->booter_dma, GFP_KERNEL);
	if (!work->booter_cpu)
		return GU_RESULT(-ENOMEM);
	memcpy(work->booter_cpu, work->booter_fw->data + 72, work->booter_image_size);
	work->v67_cpu = diag_dma_alloc(work, "v67", GU_V67_SIZE, &work->v67_dma, GFP_KERNEL);
	work->meta_cpu = diag_dma_alloc(work, "metadata", GU_WPR_META_SIZE, &work->meta_dma, GFP_KERNEL);
	if (!work->v67_cpu || !work->meta_cpu)
		return GU_RESULT(-ENOMEM);
	memcpy(work->v67_cpu, work->v67_fw->data, GU_V67_SIZE);
	work->stock_size = work->sig_span.size;
	work->stock_cpu = diag_dma_alloc(work, "stock-signature", work->stock_size, &work->stock_dma, GFP_KERNEL);
	work->boot_cpu = diag_dma_alloc(work, "boot-image", work->boot_desc.image_size, &work->boot_dma, GFP_KERNEL);
	if (!work->stock_cpu || !work->boot_cpu)
		return GU_RESULT(-ENOMEM);
	memcpy(work->stock_cpu, work->gsp_fw->data + work->sig_span.offset, work->stock_size);
	memcpy(work->boot_cpu, work->boot_fw->data, work->boot_desc.image_size);
	{
		int ret = patch_booter(work);

		return GU_RESULT(ret ? ret : unlock_transaction_radix(work));
	}
}

static int unlock_transaction_fwsec(struct unlock_transaction_work *work)
{
	static const struct falcon_eng gsp = {
		.base = 0x00110000, .fbif = 0x00110600, .falcon2 = 0x00111000,
	};
	size_t mapped = 0;
	u8 *rom = NULL;
	struct gpu_unlock_fwsec fw;
	u32 fuse_raw, version, scratch, wpr_lo, wpr_hi;
	u16 command;
	u64 frts = work->layout.frts_offset;
	void *image = NULL;
	dma_addr_t image_dma = 0;
	int ret;

	/* kgspExtractVbiosFromRom_TU102 reads NV_PROM_DATA, up to 1 MiB.
	 * pci_map_rom stops at the PCI last-image flag and hides the VN extension. */
	dev_info(&work->pdev->dev, "FWSEC: reading PROM at %#x\n", 0x300000);
	if (work->bar_len < 0x300000 + 0x1a)
		return GU_RESULT(-ENODEV);
	mapped = min_t(size_t, 1024 * 1024, work->bar_len - 0x300000);
	rom = kvmalloc(mapped, GFP_KERNEL);
	if (!rom)
		return GU_RESULT(-ENOMEM);
	memcpy_fromio(rom, work->bar + 0x300000, mapped);
	dev_info(&work->pdev->dev, "FWSEC: PROM bytes=%zu signature=%#x\n",
		 mapped, (unsigned int)(rom[0] | (rom[1] << 8)));
	ret = gpu_unlock_vbios_fwsec(rom, mapped, &fw);
	if (ret) {
		dev_err(&work->pdev->dev, "FWSEC-FRTS not found in board ROM (%d)\n", ret);
		goto out;
	}
	if (fw.engine_mask != 0x400) {
		dev_err(&work->pdev->dev, "FWSEC: unsupported GSP engine mask=%#x\n", fw.engine_mask);
		ret = -EOPNOTSUPP;
		goto out;
	}
	image = diag_dma_alloc(work, "fwsec", fw.image_size, &image_dma, GFP_KERNEL);
	if (!image) {
		ret = -ENOMEM;
		goto out;
	}
	work->fwsec_cpu = image;
	work->fwsec_dma = image_dma;
	work->fwsec_size = fw.image_size;
	memcpy(image, rom + fw.image_off, fw.image_size);
	ret = gpu_unlock_fwsec_patch_frts(image, &fw, frts);
	if (ret)
		goto out;
	if (!fw.ucode_id ||
	    read_bar(work, 0x008241c0 + (fw.ucode_id - 1) * 4, &fuse_raw)) {
		ret = -EIO;
		goto out;
	}
	version = gpu_unlock_fuse_version(fuse_raw);
	dev_info(&work->pdev->dev, "FWSEC fuse: raw=%#x version=%u\n", fuse_raw, version);
	{
		u32 sig_at = 0;

		ret = gpu_unlock_fwsec_sig_offset(fw.sig_versions, version, 384, &sig_at);
		if (ret)
			goto out;
		memcpy((u8 *)image + fw.data_off + fw.pkc_off, rom + fw.sig_off + sig_at, 384);
	}
#ifdef GU_ENTRY_LATCHED
	ret = check_entry_selectors(work);
	if (ret)
		goto out;
#endif
	pci_set_master(work->pdev);
	work->master = true;
	if (diag_pci_word(work, false, PCI_COMMAND, &command) ||
	    command == 0xffff || !(command & PCI_COMMAND_MASTER)) {
		ret = -EIO;
		goto out;
	}
#ifdef GU_ENTRY_LATCHED
	ret = check_entry_selectors(work);
	if (ret)
		goto out;
#endif
	/* Conservative lifetime barrier, not evidence of a CPU start or DMA:
	 * falcon_hs still validates geometry before its first GPU reset, and only
	 * later publishes DMA. Any failure from here retains buffers (P2). */
	work->firmware_started = true;
#ifdef GU_UEFI_PORT
	GuStarted();
#endif
	ret = falcon_hs(work, &gsp, 0, fw.imem_size, fw.imem_va, fw.imem_pa,
			fw.data_off, fw.dmem_size, fw.dmem_pa, fw.pkc_off,
			fw.engine_mask, fw.ucode_id,
			image_dma, 0, 0, false);
	if (ret)
		goto out;
	if (read_bar(work, 0x00001400 + 0x0e * 4, &scratch)) {
		ret = -EIO;
		goto out;
	}
	dev_info(&work->pdev->dev, "FWSEC result: scratch=%#x expected_hi16=0\n", scratch);
	if (scratch >> 16) {
		ret = -EIO;
		goto out;
	}
	if (read_bar(work, 0x001fa824, &wpr_lo) || wpr_lo == ~0u ||
	    read_bar(work, 0x001fa828, &wpr_hi) || wpr_hi == ~0u) {
		ret = -EIO;
		goto out;
	}
	dev_info(&work->pdev->dev, "FWSEC WPR2: low=%#x high=%#x expected_page=%#llx end=%#llx\n",
		wpr_lo, wpr_hi, (unsigned long long)(frts >> 12),
		(unsigned long long)work->layout.wpr_end);
	/* Match NVIDIA's initialized-WPR2 check as well as the expected low
	 * address. Never publish Booter metadata for disabled/alien WPR2. */
	if (!(wpr_hi >> 4) || (wpr_hi >> 4) < (wpr_lo >> 4) ||
	    ((u64)(wpr_hi >> 4) << 12) >= work->layout.wpr_end ||
	    (wpr_lo >> 4) != (u32)(frts >> 12))
		ret = -EIO;
out:
	/* Ownership transfers to work before publication, including failure paths. */
	kvfree(rom);
	return GU_RESULT(ret);
}

static int unlock_transaction_gfw(struct unlock_transaction_work *work)
{
	ktime_t deadline = ktime_add_us(ktime_get(), GU_GFW_TIMEOUT_US);
	int assessed = -EAGAIN;

	for (;;) {
		struct gpu_unlock_gfw_sample sample = { };
		u16 vendor = 0xffff;

		if (read_bar(work, GU_GFW_PLM_OFFSET, &sample.plm) ||
		    read_bar(work, GU_GFW_BOOT_OFFSET, &sample.boot) ||
		    read_bar(work, GU_GFW_CPUCTL_OFFSET, &sample.cpuctl))
			return GU_RESULT(-EIO);
		if (!diag_pci_word(work, false, PCI_VENDOR_ID, &vendor) &&
		    vendor != 0xffff)
			sample.config_ok = true;
		assessed = gpu_unlock_gfw_assess(&sample);
		if (!assessed) {
			u32 ss0, ss1, wpr_hi;

			if (read_bar(work, GU_FEAT_PLM_OFFSET, &work->plm_entry) ||
			    read_bar(work, GU_FEAT_SS0_OFFSET, &ss0) || ss0 == ~0u ||
			    read_bar(work, GU_FEAT_SS1_OFFSET, &ss1) || ss1 == ~0u ||
			    read_bar(work, 0x001fa828, &wpr_hi) || inaccessible(wpr_hi))
				return GU_RESULT(-ENODEV);
			dev_info(&work->pdev->dev, "entry: plm=%#x ss0=%#x ss1=%#x wpr2_hi=%#x\n",
				 work->plm_entry, ss0, ss1, wpr_hi);
			/* An open PLM means the card was already unlocked or not cold. */
			if (work->plm_entry == ~0u)
				return GU_RESULT(-EALREADY);
			/* WPR2 is down from power-on until FWSEC-FRTS runs. An enabled
			 * WPR2 means a driver has already used the card this power
			 * cycle; starting FWSEC and Booter again from here is untested.
			 */
			if (wpr_hi >> 4) {
				dev_err(&work->pdev->dev, "card already initialised since power on (WPR2 enabled); power off first\n");
				return GU_RESULT(-EBUSY);
			}
#ifdef GU_ENTRY_LATCHED
			if (!Gu.profile_verified || !GuEntrySelectorsMatch(ss0, ss1) ||
			    inaccessible(ss0) || ss0 == GU_SS0_UNLOCK || ss1 > 0xf ||
			    work->plm_entry != 0xffffff8f || sample.plm != 0x8b8f ||
			    sample.boot != 0x3ff || sample.cpuctl != 0x10 || wpr_hi)
				return GU_RESULT(-EIO);
			work->ss0_entry = ss0;
			work->ss1_entry = ss1;
			work->selectors_saved = true;
#endif
			return 0;
		}
		if (assessed != -EAGAIN)
			return GU_RESULT(assessed);
		if (ktime_compare(ktime_get(), deadline) >= 0)
			return GU_RESULT(-ETIMEDOUT);
		usleep_range(1000, 2000);
	}
}

static int unlock_transaction_boot(struct unlock_transaction_work *work)
{
	static const struct falcon_eng sec2 = {
		.base = SEC2_BASE, .fbif = SEC2_FBIF, .falcon2 = SEC2_FALCON2,
	};
	const struct gpu_unlock_booter_hdr *h = &work->booter_hdr;

	if (!work->published || !work->master || !work->firmware_started)
		return GU_RESULT(-EINVAL);
	return GU_RESULT(falcon_hs(work, &sec2, h->app_code_off, h->app_code_size,
			h->app_code_off, 0, h->os_data_off, h->os_data_size, 0,
			h->patch_loc - h->os_data_off, h->engine_id, h->ucode_id,
			work->booter_dma, lower_32_bits(work->meta_dma),
			upper_32_bits(work->meta_dma), true));
}

static void put_le64(u8 *dest, u64 value)
{
	unsigned int i;

	for (i = 0; i < 8; i++)
		dest[i] = (u8)(value >> (8 * i));
}

static int unlock_transaction_signature(struct unlock_transaction_work *work)
{
	u8 *meta = work->meta_cpu;

	if (!meta || !work->stock_cpu || !work->pages_allocated ||
	    work->signature_calls >= 2)
		return GU_RESULT(-EINVAL);
	if (work->signature_calls == 0) {
		struct gpu_unlock_wpr_args args = {
			.fb_size = work->layout_in.usable_fb,
			.vga_offset = work->layout_in.vga_base,
			.frts_size = work->layout_in.frts_size,
			.signature_dma = work->v67_dma, .signature_size = GU_V67_SIZE,
			.radix_dma = work->pages[0].dma,
			.image_size = work->image_span.size,
			.boot_dma = work->boot_dma, .boot = work->boot_desc,
		};
		int ret = gpu_unlock_wpr_meta_pack(meta, &work->layout, &args);

		if (ret)
			return GU_RESULT(ret);
		dma_wmb();
		work->published = true;
	} else {
		put_le64(meta + 72, work->stock_dma);
		put_le64(meta + 80, work->stock_size);
		dma_wmb();
	}
	work->signature_calls++;
	return 0;
}

#ifndef GU_ENTRY_LATCHED
/* Write the unlock value to one speed selector and read it back. */
static int write_selector(struct unlock_transaction_work *work, const char *name,
			  u32 offset, u32 unlock)
{
	u32 value;

	if (read_bar(work, offset, &value) || inaccessible(value))
		return GU_RESULT(-ENODEV);
	dev_info(&work->pdev->dev, "selector read: %s=%#x expected_unlock=%#x\n",
		 name, value, unlock);
	if (value == unlock)
		return 0;
	dev_info(&work->pdev->dev, "selector write: %s=%#x\n", name, unlock);
	write_bar(work, offset, unlock);
	if (read_bar(work, offset, &value) || value == ~0u)
		return GU_RESULT(-ENODEV);
	return GU_RESULT(value == unlock ? 0 : -EIO);
}
#endif

/* SS0 selects the full BF16 rate. Entry-latched modes keep the cold snapshot
 * through the SS0 write; the full policy then permits one checked SS1 write. */
static int unlock_transaction_selector(struct unlock_transaction_work *work)
{
#ifdef GU_ENTRY_LATCHED_SS1_PRESERVE
	u32 value, ss1;
	int ret;

	dev_info(&work->pdev->dev, "selector intent: SS0target=%#x SS1preserve=%#x (no SS1 write)\n",
		 GU_SS0_UNLOCK, work->ss1_entry);
	/* Compare after logging, immediately before the only selector write. */
	ret = check_entry_selectors(work);
	if (ret)
		return ret;
	write_bar(work, GU_FEAT_SS0_OFFSET, GU_SS0_UNLOCK);
	if (read_bar(work, GU_FEAT_SS0_OFFSET, &value) ||
	    read_bar(work, GU_FEAT_SS1_OFFSET, &ss1) ||
	    value != GU_SS0_UNLOCK || ss1 != work->ss1_entry)
		return GU_RESULT(-EIO);
	return 0;
#elif defined(GU_ENTRY_LATCHED_SS1_FULL)
	u32 value;
	int ret;

	dev_info(&work->pdev->dev,
		 "selector intent: SS0target=%#x SS1entry=%#x SS1target=%#x (one SS1 write)\n",
		 GU_SS0_UNLOCK, work->ss1_entry, GU_SS1_UNLOCK);
	ret = check_entry_selectors(work);
	if (ret)
		return ret;
	write_bar(work, GU_FEAT_SS0_OFFSET, GU_SS0_UNLOCK);
	if (read_bar(work, GU_FEAT_SS0_OFFSET, &value) || value != GU_SS0_UNLOCK ||
	    !GuSelectorPhase(GU_SELECTOR_PHASE_SS0_WRITTEN))
		return GU_RESULT(-EIO);
	if (read_bar(work, GU_FEAT_SS1_OFFSET, &value) || value != work->ss1_entry ||
	    !GuSelectorPhase(GU_SELECTOR_PHASE_SS1_WRITE))
		return GU_RESULT(-EIO);
	write_bar(work, GU_FEAT_SS1_OFFSET, GU_SS1_UNLOCK);
	if (read_bar(work, GU_FEAT_SS1_OFFSET, &value) || value != GU_SS1_UNLOCK ||
	    !GuSelectorPhase(GU_SELECTOR_PHASE_TARGET))
		return GU_RESULT(-EIO);
	return 0;
#else
	int ret = write_selector(work, "SS0", GU_FEAT_SS0_OFFSET, GU_SS0_UNLOCK);

	return ret ? ret : write_selector(work, "SS1", GU_FEAT_SS1_OFFSET, GU_SS1_UNLOCK);
#endif
}

/* Reset SEC2 after FLR and leave it halted with reset released. Do not reset
 * GSP here: stock RM first requires completed GFW and a halted GSP Falcon.
 * ksec2ResetHw_TU102 ends with RESET_FALSE. Leaving RESET_TRUE makes later
 * host reads return FECS_PRI_RESET (0xbadf12 in bits 31:8). RM's scrub wait
 * treats that value as scrubbing-pending and does not finish.
 */
static int hold_sec2_reset(struct unlock_transaction_work *work, const char *phase)
{
	unsigned int j;
	u32 engine, before, first = 0, hwcfg;
	int ret;

	if (read_bar(work, SEC2_BASE + 0x3c0, &engine) || inaccessible(engine))
		return GU_RESULT(-ENODEV);
	before = engine;
	dev_info(&work->pdev->dev, "engine reset assert: phase=%s base=%#x before=%#x write=%#x\n",
		 phase, SEC2_BASE, before, before | 1);
	write_bar(work, SEC2_BASE + 0x3c0, engine | 1);
	/* 610 ksec2ResetHw_TU102 requires ten register cycles for signal
	 * propagation. A stale zero during those cycles is not evidence that
	 * the assertion failed. No CPU start and no reset retry.
	 */
	for (j = 0; j < 10; j++) {
		if (read_bar(work, SEC2_BASE + 0x3c0, &engine) ||
		    inaccessible(engine)) {
			dev_err(&work->pdev->dev, "engine reset inaccessible: phase=%s base=%#x cycle=%u value=%#x\n",
				phase, SEC2_BASE, j + 1, engine);
			return GU_RESULT(-EIO);
		}
		if (!j)
			first = engine;
	}
	dev_info(&work->pdev->dev, "engine reset propagation: phase=%s base=%#x first=%#x tenth=%#x\n",
		 phase, SEC2_BASE, first, engine);
	ret = poll_us(work, SEC2_BASE + 0x3c0, 1, 1, 1000, true);
	dev_info(&work->pdev->dev, "engine reset asserted: phase=%s base=%#x error=%d last=%#x\n",
		 phase, SEC2_BASE, ret, work->last_value);
	if (ret)
		return GU_RESULT(ret);
	if (read_bar(work, SEC2_BASE + 0x3c0, &engine) || inaccessible(engine))
		return GU_RESULT(-EIO);
	dev_info(&work->pdev->dev, "engine reset release: phase=%s base=%#x before=%#x write=%#x\n",
		 phase, SEC2_BASE, engine, engine & ~1u);
	write_bar(work, SEC2_BASE + 0x3c0, engine & ~1u);
	for (j = 0; j < 10; j++) {
		if (read_bar(work, SEC2_BASE + 0x3c0, &engine) ||
		    inaccessible(engine)) {
			dev_err(&work->pdev->dev, "engine reset release inaccessible: phase=%s base=%#x cycle=%u value=%#x\n",
				phase, SEC2_BASE, j + 1, engine);
			return GU_RESULT(-EIO);
		}
	}
	ret = poll_us(work, SEC2_BASE + 0x3c0, 1, 0, 1000, true);
	if (ret)
		return GU_RESULT(ret);
	/* NV_PFALCON_FALCON_HWCFG2 bit 12 clears when memory scrubbing finishes.
	 * A PRI error fails this wait instead of being handed to RM.
	 */
	ret = poll_us(work, SEC2_BASE + 0xf4, 1u << 12, 0, 1000000, true);
	if (ret)
		return GU_RESULT(ret);
	if (read_bar(work, SEC2_BASE + 0xf4, &hwcfg) || inaccessible(hwcfg))
		return GU_RESULT(-EIO);
	dev_info(&work->pdev->dev, "engine reset released: phase=%s base=%#x engine=%#x hwcfg2=%#x\n",
		 phase, SEC2_BASE, engine, hwcfg);
	return 0;
}

/* PCI configuration can be ready before BAR0. The 610 BIF restore path waits
 * for PMC_BOOT_0 to match the saved chip ID before touching engine registers.
 * Transient badf/all-ones reads are expected only inside this bounded wait.
 */
static int wait_post_flr_bar(struct unlock_transaction_work *work, u32 chip)
{
	ktime_t deadline = ktime_add_us(ktime_get(), 1000000);
	u32 value = 0;

	for (;;) {
		if (read_bar(work, 0, &value))
			return GU_RESULT(-EIO);
		if (value == chip) {
			dev_info(&work->pdev->dev, "post-FLR BAR ready: chip=%#x\n", value);
			return 0;
		}
		if (ktime_compare(ktime_get(), deadline) >= 0) {
			dev_err(&work->pdev->dev, "post-FLR BAR timeout: chip=%#x expected=%#x budget_us=1000000\n",
				value, chip);
			return GU_RESULT(-ETIMEDOUT);
		}
		usleep_range(1000, 2000);
	}
}

/* Match gpuWaitForGfwBootComplete_TU102, without launching any firmware or
 * overwriting the cold-entry evidence. The post-FLR ROM boot may still be busy.
 */
static int wait_post_flr_gfw(struct unlock_transaction_work *work)
{
	ktime_t deadline = ktime_add_us(ktime_get(), GU_GFW_TIMEOUT_US);
	struct gpu_unlock_gfw_sample sample = { };
	u32 identity = 0;
	int ret;

	for (;;) {
		sample = (struct gpu_unlock_gfw_sample){ };
		ret = diag_pci_dword(work, false, PCI_VENDOR_ID, &identity);
		sample.config_ok = !ret && identity == GU_TARGET_PCI_ID;
		if (!sample.config_ok) {
			ret = -ENODEV;
			break;
		}
		if (read_bar(work, GU_GFW_CPUCTL_OFFSET, &sample.cpuctl) ||
		    read_bar(work, GU_GFW_PLM_OFFSET, &sample.plm)) {
			ret = -EIO;
			break;
		}
		/* Avoid the protected scratch read until PL0 can read it. */
		if (!inaccessible(sample.plm) && (sample.plm & GU_GFW_PLM_LEVEL0) &&
		    read_bar(work, GU_GFW_BOOT_OFFSET, &sample.boot)) {
			ret = -EIO;
			break;
		}
		ret = inaccessible(sample.plm) || inaccessible(sample.boot) ||
		      inaccessible(sample.cpuctl) ? -EAGAIN : gpu_unlock_gfw_assess(&sample);
		if (!ret)
			break;
		if (ktime_compare(ktime_get(), deadline) >= 0) {
			ret = -ETIMEDOUT;
			break;
		}
		usleep_range(1000, 2000);
	}
	dev_info(&work->pdev->dev, "post-FLR GFW result: error=%d pci=%#x plm=%#x boot=%#x cpuctl=%#x budget_us=%u\n",
		 ret, identity, sample.plm, sample.boot, sample.cpuctl, GU_GFW_TIMEOUT_US);
	return GU_RESULT(ret);
}

static int unlock_transaction_reset(struct unlock_transaction_work *work)
{
	u32 identity, bar, lo, hi, ss0, ss1, plm, chip, cpuctl;
	u16 command, pending;
	unsigned int i;
	int ret;

	if (work->signature_calls != 2)
		return GU_RESULT(-EPROTO);
	if (read_bar(work, GU_FEAT_SS0_OFFSET, &ss0) || ss0 == ~0u ||
	    read_bar(work, GU_FEAT_SS1_OFFSET, &ss1) || ss1 == ~0u)
		return GU_RESULT(-ENODEV);
	dev_info(&work->pdev->dev, "pre-FLR selectors: ss0=%#x expected=%#x ss1=%#x expected=%#x\n",
		ss0, GU_SS0_UNLOCK, ss1, GU_SS1_EXPECTED(work));
	if (ss0 != GU_SS0_UNLOCK || ss1 != GU_SS1_EXPECTED(work))
		return GU_RESULT(-EIO);
	pci_clear_master(work->pdev);
	work->master = false;
	if (diag_pci_word(work, false, PCI_COMMAND, &command) ||
	    command == 0xffff || (command & PCI_COMMAND_MASTER))
		return GU_RESULT(-EIO);
	if (read_bar(work, 0, &chip) || !chip || inaccessible(chip))
		return GU_RESULT(-ENODEV);
	/* Ampere PF-FLR requires no engine-reset preconditioning in this cold,
	 * unbound path (no initialized NVLink/P2P). The pinned GA102 rejoin14
	 * sequence also goes from the early PLM result to FLR. A pre-FLR engine
	 * hold is not a prerequisite: secure firmware can reject that write.
	 * Keep EVERY allocation alive, BME off, until post-FLR containment.
	 */
	/* FLR resets the display engine, so a firmware framebuffer (simpledrm,
	 * efifb) on this card must go first; nvidia-drm replaces it later. This
	 * does nothing on a card without one.
	 */
	ret = aperture_remove_conflicting_pci_devices(work->pdev, "gpu_unlock");
	if (ret)
		return GU_RESULT(ret);
	dev_info(&work->pdev->dev, "PCIe FLR begin: BME=0 DMA retained, chip=%#x\n", chip);
	ret = pcie_flr(work->pdev);
	dev_info(&work->pdev->dev, "PCIe FLR returned: result=%d\n", ret);
	if (ret)
		return GU_RESULT(ret);
	/* The saved image predates BME. No transient restore with BME set. */
	pci_restore_state(work->pdev);
	if (diag_pci_dword(work, false, PCI_VENDOR_ID, &identity) ||
	    identity != GU_TARGET_PCI_ID ||
	    diag_pci_word(work, false, PCI_COMMAND, &command) ||
	    command == 0xffff || (command & PCI_COMMAND_MASTER) ||
	    !(command & PCI_COMMAND_MEMORY))
		return GU_RESULT(-ENODEV);
	for (i = 0; i < ARRAY_SIZE(work->bars); i++) {
		if (diag_pci_dword(work, false, PCI_BASE_ADDRESS_0 + i * 4, &bar) ||
		    bar != work->bars[i]) {
			dev_err(&work->pdev->dev, "post-FLR BAR%u: actual=%#x expected=%#x\n", i, bar, work->bars[i]);
			return GU_RESULT(-EIO);
		}
	}
	ret = wait_post_flr_bar(work, chip);
	if (ret)
		return GU_RESULT(ret);
	/* ROM GFW completion/halt plus FLR disposes of the old GSP execution.
	 * Stock RM checks this halt before resetting GSP with its own buffers.
	 */
	ret = wait_post_flr_gfw(work);
	if (ret)
		return GU_RESULT(ret);
	ret = hold_sec2_reset(work, "post-FLR");
	if (ret)
		return GU_RESULT(ret);
	if (diag_pci_word(work, true, PCI_EXP_DEVSTA, &pending) ||
	    pending == 0xffff || (pending & PCI_EXP_DEVSTA_TRPND))
		return GU_RESULT(-EBUSY);
	if (read_bar(work, GU_GFW_CPUCTL_OFFSET, &cpuctl) || inaccessible(cpuctl) ||
	    !(cpuctl & GU_GFW_CPUCTL_HALTED) ||
	    read_bar(work, 0x001fa824, &lo) || inaccessible(lo) ||
	    read_bar(work, 0x001fa828, &hi) || inaccessible(hi) ||
	    read_bar(work, GU_FEAT_SS0_OFFSET, &ss0) || inaccessible(ss0) ||
	    read_bar(work, GU_FEAT_SS1_OFFSET, &ss1) || inaccessible(ss1) ||
	    read_bar(work, GU_FEAT_PLM_OFFSET, &plm) ||
	    (plm & 0xffff0000u) == 0xbadf0000u)
		return GU_RESULT(-ENODEV);
	dev_info(&work->pdev->dev,
		 "post-FLR SEC2 reset released, GSP halted, BME=0 pending=0 WPR2=%08x:%08x SS0=%08x SS1=%08x\n",
		 lo, hi, ss0, ss1);
	/* Logging can run external firmware/USB callbacks; never quiesce from the
	 * samples collected before that callback. Entry selectors must survive FLR.
	 */
	if (diag_pci_word(work, false, PCI_COMMAND, &command) ||
	    command == 0xffff || (command & PCI_COMMAND_MASTER) ||
	    !(command & PCI_COMMAND_MEMORY) ||
	    diag_pci_word(work, true, PCI_EXP_DEVSTA, &pending) ||
	    pending == 0xffff || (pending & PCI_EXP_DEVSTA_TRPND) ||
	    read_bar(work, GU_GFW_CPUCTL_OFFSET, &cpuctl) || inaccessible(cpuctl) ||
	    !(cpuctl & GU_GFW_CPUCTL_HALTED) ||
	    read_bar(work, 0x001fa824, &lo) || inaccessible(lo) ||
	    read_bar(work, 0x001fa828, &hi) || inaccessible(hi) ||
	    read_bar(work, GU_FEAT_SS0_OFFSET, &ss0) || inaccessible(ss0) ||
	    read_bar(work, GU_FEAT_SS1_OFFSET, &ss1) || inaccessible(ss1) ||
	    read_bar(work, GU_FEAT_PLM_OFFSET, &plm) ||
	    (plm & 0xffff0000u) == 0xbadf0000u)
		return GU_RESULT(-ENODEV);
	/* Both 610 and installed 615 _kgspBootGspRm reject preexisting WPR2
	 * on this cold dGPU path, even our old in-bounds FRTS reservation.
	 * kgspIsWpr2Up_TU102 uses HI.VAL != 0, not a LO/HI range comparison.
	 */
	if (hi >> 4) {
		dev_err(&work->pdev->dev, "post-FLR WPR2 remains enabled: lo=%#x hi=%#x; stock driver requires HI.VAL=0\n", lo, hi);
		return GU_RESULT(-EBUSY);
	}
	if (ss0 != GU_SS0_UNLOCK || ss1 != GU_SS1_EXPECTED(work))
		return GU_RESULT(-EIO);
	work->quiesced = true;
#ifdef GU_UEFI_PORT
	GuQuiesced();
#endif
	return 0;
}

int gpu_unlock_610_load_shared(struct pci_dev *pdev, struct gpu_unlock_610_shared *shared)
{
	return request_scoped(&pdev->dev, &shared->gsp, GSP_FILE);
}

void gpu_unlock_610_release_shared(struct gpu_unlock_610_shared *shared)
{
	release_firmware(shared->gsp);
	shared->gsp = NULL;
}

int gpu_unlock_610_unlock(struct pci_dev *pdev, const struct gpu_unlock_610_shared *shared)
{
	struct unlock_transaction_work *work;
	int ret, cleanup;

	work = kzalloc(sizeof(*work), GFP_KERNEL);
	if (!work)
		return -ENOMEM;
	work->pdev = pci_dev_get(pdev);
	work->gsp_fw = shared->gsp;
	ret = unlock_transaction_assets(work);
	if (!ret)
		ret = unlock_transaction_claim(work);
	if (!ret)
		ret = unlock_transaction_layout(work);
	if (!ret)
		ret = unlock_transaction_dma(work);
	if (!ret)
		ret = unlock_transaction_gfw(work);
	if (!ret)
		ret = unlock_transaction_fwsec(work);
	if (!ret)
		ret = unlock_transaction_signature(work);	/* publish V67 */
	if (!ret)
		ret = unlock_transaction_boot(work);
	if (!ret)
		ret = unlock_transaction_signature(work);	/* restore stock */
	if (!ret)
		ret = unlock_transaction_selector(work);
	if (!ret)
		ret = unlock_transaction_reset(work);
	/* Never leave the V67 signature published for a later stock boot. */
	if (ret && work->signature_calls == 1)
		unlock_transaction_signature(work);
	cleanup = unlock_transaction_unwind(work);
	if (!ret)
		ret = cleanup;
	if (work->retained) {
		/* Firmware may still DMA into these buffers: keep them, the device
		 * reference and this module until the next power off. */
		dev_err(&pdev->dev, "firmware quiescence not proven; keeping DMA buffers until power off\n");
		__module_get(THIS_MODULE);
		return ret;
	}
	pci_dev_put(work->pdev);
	kfree(work);
	return ret;
}

MODULE_FIRMWARE(BOOTER_FILE);
MODULE_FIRMWARE(V67_FILE);
MODULE_FIRMWARE(GSP_FILE);
MODULE_FIRMWARE(BOOT_FILE);
MODULE_FIRMWARE(BOOT_DESC_FILE);
