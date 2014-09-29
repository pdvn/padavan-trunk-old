/**************************************************************************
 *
 *  BRIEF MODULE DESCRIPTION
 *     PCI init for Ralink RT2880 solution
 *
 *  Copyright 2007 Ralink Inc. (bruce_chang@ralinktech.com.tw)
 *
 *  This program is free software; you can redistribute  it and/or modify it
 *  under  the terms of  the GNU General  Public License as published by the
 *  Free Software Foundation;  either version 2 of the  License, or (at your
 *  option) any later version.
 *
 *  THIS  SOFTWARE  IS PROVIDED   ``AS  IS'' AND   ANY  EXPRESS OR IMPLIED
 *  WARRANTIES,   INCLUDING, BUT NOT  LIMITED  TO, THE IMPLIED WARRANTIES OF
 *  MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.  IN
 *  NO  EVENT  SHALL   THE AUTHOR  BE    LIABLE FOR ANY   DIRECT, INDIRECT,
 *  INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 *  NOT LIMITED   TO, PROCUREMENT OF  SUBSTITUTE GOODS  OR SERVICES; LOSS OF
 *  USE, DATA,  OR PROFITS; OR  BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
 *  ANY THEORY OF LIABILITY, WHETHER IN  CONTRACT, STRICT LIABILITY, OR TORT
 *  (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 *  THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 *  You should have received a copy of the  GNU General Public License along
 *  with this program; if not, write  to the Free Software Foundation, Inc.,
 *  675 Mass Ave, Cambridge, MA 02139, USA.
 *
 *
 **************************************************************************
 * May 2007 Bruce Chang
 * Initial Release
 *
 * May 2009 Bruce Chang
 * support RT2880/RT3883 PCIe
 *
 * May 2011 Bruce Chang
 * support RT6855/MT7620 PCIe
 *
 **************************************************************************
 */

#include <linux/types.h>
#include <linux/pci.h>
#include <linux/kernel.h>
#include <linux/version.h>
#include <linux/init.h>
#include <linux/mod_devicetable.h>
#include <linux/slab.h>
#include <linux/delay.h>

#include <asm/pci.h>
#include <asm/io.h>
#include <asm/rt2880/eureka_ep430.h>
#include <asm/rt2880/surfboardint.h>

#ifdef CONFIG_PCI

//#define RAPCI_DEBUG

#if defined (CONFIG_RT2880_DRAM_256M)
#define BAR_MASK			0x0FFF0000
#define BAR_HIGH			0x0FFFFFFF
#elif defined (CONFIG_RT2880_DRAM_128M)
#define BAR_MASK			0x07FF0000
#define BAR_HIGH			0x07FFFFFF
#elif defined (CONFIG_RT2880_DRAM_64M)
#define BAR_MASK			0x03FF0000
#define BAR_HIGH			0x03FFFFFF
#elif defined (CONFIG_RT2880_DRAM_32M)
#define BAR_MASK			0x01FF0000
#define BAR_HIGH			0x01FFFFFF
#else
#define BAR_MASK			0x00FF0000
#define BAR_HIGH			0x00FFFFFF
#endif

/*
 * These functions and structures provide the BIOS scan and mapping of the PCI
 * devices.
 */

#if defined (CONFIG_RALINK_MT7621)
#define RALINK_SYSTEM_CONTROL_BASE	0xbe000000
#define RALINK_PCI_MM_MAP_BASE		0x60000000
#define RALINK_PCI_IO_MAP_BASE		0x1e160000
#else
#define RALINK_SYSTEM_CONTROL_BASE	0xb0000000
#define RALINK_PCI_MM_MAP_BASE		0x20000000
#define RALINK_PCI_IO_MAP_BASE		0x10160000
#endif

#define MEMORY_BASE			0x0

#if defined (CONFIG_RALINK_MT7621)
extern u32 ralink_asic_rev_id;
static int pcie_link_status = 0;

/* use GPIO control instead of PERST_N */
#define GPIO_PERST

#define PCIE_SHARE_PIN_SW		10
#if defined (GPIO_PERST)
#define GPIO_PCIE_PORT0			19
#if defined CONFIG_RALINK_I2S || defined CONFIG_RALINK_I2S_MODULE
#define UARTL3_SHARE_PIN_SW		PCIE_SHARE_PIN_SW
#define GPIO_PCIE_PORT1			GPIO_PCIE_PORT0
#define GPIO_PCIE_PORT2			GPIO_PCIE_PORT0
#else
#define UARTL3_SHARE_PIN_SW		3
#define GPIO_PCIE_PORT1			8
#define GPIO_PCIE_PORT2			7
#endif
#define RALINK_GPIO_CTRL0		*(volatile u32 *)(RALINK_PIO_BASE + 0x00)
#define RALINK_GPIO_DATA0		*(volatile u32 *)(RALINK_PIO_BASE + 0x20)
#endif

#define ASSERT_SYSRST_PCIE(val)		do {	\
						if ((ralink_asic_rev_id & 0xFFFF) == 0x0101)	\
							RALINK_RSTCTRL |= val;	\
						else	\
							RALINK_RSTCTRL &= ~val;	\
					} while(0)
#define DEASSERT_SYSRST_PCIE(val)	do {	\
						if ((ralink_asic_rev_id & 0xFFFF) == 0x0101)	\
							RALINK_RSTCTRL &= ~val;	\
						else	\
							RALINK_RSTCTRL |= val;	\
					} while(0)
#endif

#define RALINK_SYSCFG1 			*(volatile u32 *)(RALINK_SYSTEM_CONTROL_BASE + 0x14)
#define RALINK_CLKCFG1			*(volatile u32 *)(RALINK_SYSTEM_CONTROL_BASE + 0x30)
#define RALINK_RSTCTRL			*(volatile u32 *)(RALINK_SYSTEM_CONTROL_BASE + 0x34)
#define RALINK_GPIOMODE			*(volatile u32 *)(RALINK_SYSTEM_CONTROL_BASE + 0x60)
#define RALINK_PCIE_CLK_GEN		*(volatile u32 *)(RALINK_SYSTEM_CONTROL_BASE + 0x7c)
#define RALINK_PCIE_CLK_GEN1		*(volatile u32 *)(RALINK_SYSTEM_CONTROL_BASE + 0x80)
#define PPLL_CFG1			*(volatile u32 *)(RALINK_SYSTEM_CONTROL_BASE + 0x9c)
#define PPLL_DRV			*(volatile u32 *)(RALINK_SYSTEM_CONTROL_BASE + 0xa0)
//RALINK_SYSCFG1 bit
#define RALINK_PCI_HOST_MODE_EN		(1<<7)
#define RALINK_PCIE_RC_MODE_EN		(1<<8)
//RALINK_RSTCTRL bit
#define RALINK_PCIE_RST			(1<<23)
#define RALINK_PCI_RST			(1<<24)
//RALINK_CLKCFG1 bit
#define RALINK_PCI_CLK_EN		(1<<19)
#define RALINK_PCIE_CLK_EN		(1<<21)
//RALINK_GPIOMODE bit
#define PCI_SLOTx2			(1<<11)
#define PCI_SLOTx1			(2<<11)
//MTK PCIE PLL bit
#define PDRV_SW_SET			(1<<31)
#define LC_CKDRVPD			(1<<19)
#define LC_CKDRVOHZ			(1<<18)
#define LC_CKDRVHZ			(1<<17)

#define PCI_ACCESS_READ_1		0
#define PCI_ACCESS_READ_2		1
#define PCI_ACCESS_READ_4		2
#define PCI_ACCESS_WRITE_1		3
#define PCI_ACCESS_WRITE_2		4
#define PCI_ACCESS_WRITE_4		5

static int config_access(int access_type, u32 busn, u32 slot, u32 func, u32 where, u32 *data)
{
	u32 address_reg, data_reg, address;

	address_reg = RALINK_PCI_CONFIG_ADDR;
	data_reg = RALINK_PCI_CONFIG_DATA_VIRTUAL_REG;

#if defined(CONFIG_RALINK_RT3883)
	if (busn == 0)
		where &= 0xff; // high bits used only for RT3883 PCIe bus (busn 1)
#endif

	/* Setup address */
	address = 0x80000000 | (((where & 0xf00)>>8)<<24) | (busn << 16) | (slot << 11) | (func << 8) | (where & 0xfc);

	/* start the configuration cycle */
	MV_WRITE(address_reg, address);

	switch (access_type) {
	case PCI_ACCESS_WRITE_1:
		MV_WRITE_8(data_reg + (where & 0x3), *data);
		break;
	case PCI_ACCESS_WRITE_2:
		MV_WRITE_16(data_reg + (where & 0x3), *data);
		break;
	case PCI_ACCESS_WRITE_4:
		MV_WRITE(data_reg, *data);
		break;
	case PCI_ACCESS_READ_1:
		MV_READ_8(data_reg + (where & 0x3), data);
		break;
	case PCI_ACCESS_READ_2:
		MV_READ_16(data_reg + (where & 0x3), data);
		break;
	case PCI_ACCESS_READ_4:
		MV_READ(data_reg, data);
		break;
	}

	return PCIBIOS_SUCCESSFUL;
}

static int ralink_pci_config_read(struct pci_bus *bus, unsigned int devfn, int where, int size, u32 *val)
{
	u32 busn = bus->number;
	u32 slot = PCI_SLOT(devfn);
	u32 func = PCI_FUNC(devfn);

	switch (size) {
	case 1:
		return config_access(PCI_ACCESS_READ_1, busn, slot, func, (u32)where, val);
	case 2:
		return config_access(PCI_ACCESS_READ_2, busn, slot, func, (u32)where, val);
	default:
		return config_access(PCI_ACCESS_READ_4, busn, slot, func, (u32)where, val);
	}
}

static int ralink_pci_config_write(struct pci_bus *bus, unsigned int devfn, int where, int size, u32 val)
{
	u32 busn = bus->number;
	u32 slot = PCI_SLOT(devfn);
	u32 func = PCI_FUNC(devfn);

	switch (size) {
	case 1:
		return config_access(PCI_ACCESS_WRITE_1, busn, slot, func, (u32)where, &val);
	case 2:
		return config_access(PCI_ACCESS_WRITE_2, busn, slot, func, (u32)where, &val);
	default:
		return config_access(PCI_ACCESS_WRITE_4, busn, slot, func, (u32)where, &val);
	}
}

/*
 *  General-purpose PCI functions.
 */

struct pci_ops ralink_pci_ops = {
	.read		 = ralink_pci_config_read,
	.write		 = ralink_pci_config_write,
};

static struct resource ralink_res_pci_mem1 = {
	.name		 = "PCI MEM1",
	.start		 = RALINK_PCI_MM_MAP_BASE,
	.end		 = (RALINK_PCI_MM_MAP_BASE + 0x0fffffff),
	.flags		 = IORESOURCE_MEM,
};

static struct resource ralink_res_pci_io1 = {
	.name		 = "PCI I/O1",
	.start		 = RALINK_PCI_IO_MAP_BASE,
	.end		 = (RALINK_PCI_IO_MAP_BASE + 0x0ffff),
	.flags		 = IORESOURCE_IO,
};

struct pci_controller ralink_pci_controller = {
	.pci_ops	 = &ralink_pci_ops,
	.mem_resource	 = &ralink_res_pci_mem1,
	.io_resource	 = &ralink_res_pci_io1,
	.mem_offset	 = 0x00000000UL,
	.io_offset	 = 0x00000000UL,
};

struct pci_fixup pcibios_fixups[] = {
	{0}
};

#if LINUX_VERSION_CODE < KERNEL_VERSION(3,2,0)
int __init pcibios_map_irq(struct pci_dev *dev, u8 slot, u8 pin)
#else
int __init pcibios_map_irq(const struct pci_dev *dev, u8 slot, u8 pin)
#endif
{
	int pci_irq;
	u16 cmd;
	u8 pci_latency, pci_cache_line;
#ifdef RAPCI_DEBUG
	struct resource *res;
	int i;
	u32 val;
#endif

	pci_irq = 0;
	pci_latency = 0xff;
	pci_cache_line = (L1_CACHE_BYTES >> 2);

#ifdef RAPCI_DEBUG
	printk("** bus = %d, slot = 0x%x\n", dev->bus->number, slot);
#endif

#if defined (CONFIG_RALINK_MT7621)
	if ((dev->bus->number == 0) && (slot == 0x0)) {
		pci_write_config_dword(dev, PCI_BASE_ADDRESS_0, MEMORY_BASE);
#ifdef RAPCI_DEBUG
		pci_read_config_dword(dev, PCI_IO_BASE, &val);
		printk("PCI_IO_BASE: 0x%08X\n", val);
#endif
	} else if ((dev->bus->number == 0) && (slot == 0x1)) {
		pci_write_config_dword(dev, PCI_BASE_ADDRESS_0, MEMORY_BASE);
#ifdef RAPCI_DEBUG
		pci_read_config_dword(dev, PCI_IO_BASE, &val);
		printk("PCI_IO_BASE: 0x%08X\n", val);
#endif
	} else if ((dev->bus->number == 0) && (slot == 0x2)) {
		pci_write_config_dword(dev, PCI_BASE_ADDRESS_0, MEMORY_BASE);
#ifdef RAPCI_DEBUG
		pci_read_config_dword(dev, PCI_IO_BASE, &val);
		printk("PCI_IO_BASE: 0x%08X\n", val);
#endif
	} else if ((dev->bus->number == 1) && (slot == 0x0)) {
		switch (pcie_link_status) {
		case 2:
		case 6:
			pci_irq = SURFBOARDINT_PCIE1;
			break;
		case 4:
			pci_irq = SURFBOARDINT_PCIE2;
			break;
		default:
			pci_irq = SURFBOARDINT_PCIE0;
		}
		pci_cache_line = 0; // not available for PCIe
	} else if ((dev->bus->number == 2) && (slot == 0x0)) {
		switch (pcie_link_status) {
		case 5:
		case 6:
			pci_irq = SURFBOARDINT_PCIE2;
			break;
		default:
			pci_irq = SURFBOARDINT_PCIE0;
		}
		pci_cache_line = 0; // not available for PCIe
	} else if ((dev->bus->number == 2) && (slot == 0x1)) {
		switch (pcie_link_status) {
		case 5:
		case 6:
			pci_irq = SURFBOARDINT_PCIE2;
			break;
		default:
			pci_irq = SURFBOARDINT_PCIE0;
		}
		pci_cache_line = 0; // not available for PCIe
	} else if ((dev->bus->number == 3) && (slot == 0x0)) {
		pci_irq = SURFBOARDINT_PCIE2;
		pci_cache_line = 0; // not available for PCIe
	} else if ((dev->bus->number == 3) && (slot == 0x1)) {
		pci_irq = SURFBOARDINT_PCIE2;
		pci_cache_line = 0; // not available for PCIe
	} else if ((dev->bus->number == 3) && (slot == 0x2)) {
		pci_irq = SURFBOARDINT_PCIE2;
		pci_cache_line = 0; // not available for PCIe
	} else {
		return 0;
	}
#elif defined (CONFIG_RALINK_MT7620) || defined(CONFIG_RALINK_MT7628)
	if ((dev->bus->number == 0) && (slot == 0x0)) {
		pci_write_config_dword(dev, PCI_BASE_ADDRESS_0, MEMORY_BASE);
#ifdef RAPCI_DEBUG
		pci_read_config_dword(dev, PCI_IO_BASE, &val);
		printk("PCI_IO_BASE: 0x%08X\n", val);
#endif
	} else if ((dev->bus->number == 1) && (slot == 0x0)) {
		pci_irq = SURFBOARDINT_PCIE0;
		pci_cache_line = 0; // not available for PCIe
	} else {
		return 0;
	}
#elif defined (CONFIG_RALINK_RT3883)
	if ((dev->bus->number == 0) && (slot == 0x0)) {
		pci_write_config_dword(dev, PCI_BASE_ADDRESS_0, MEMORY_BASE);
#if defined (CONFIG_PCIE_ONLY)
		pci_write_config_dword(dev, PCI_IO_BASE, 0x00000101);
#ifdef RAPCI_DEBUG
		pci_read_config_dword(dev, PCI_IO_BASE, &val);
		printk("PCI_IO_BASE: 0x%08X\n", val);
#endif
#endif
	} else if ((dev->bus->number == 0) && (slot == 0x1)) {
		pci_write_config_dword(dev, PCI_IO_BASE, 0x00000101);
#ifdef RAPCI_DEBUG
		pci_read_config_dword(dev, PCI_IO_BASE, &val);
		printk("PCI_IO_BASE: 0x%08X\n", val);
#endif
	} else if ((dev->bus->number == 0) && (slot == 0x11)) {
		pci_irq = SURFBOARDINT_PCI0;
		pci_latency = 64;
	} else if ((dev->bus->number == 0) && (slot == 0x12)) {
		pci_irq = SURFBOARDINT_PCI1;
		pci_latency = 64;
	} else if ((dev->bus->number == 1)) {
		pci_irq = SURFBOARDINT_PCIE0;
		pci_cache_line = 0; // not available for PCIe
	} else {
		return 0;
	}
#endif

	if (pci_cache_line)
		pci_write_config_byte(dev, PCI_CACHE_LINE_SIZE, pci_cache_line);

	pci_read_config_word(dev, PCI_COMMAND, &cmd);
	cmd |= (PCI_COMMAND_MASTER | PCI_COMMAND_IO | PCI_COMMAND_MEMORY);
	pci_write_config_word(dev, PCI_COMMAND, cmd);

	pci_write_config_byte(dev, PCI_LATENCY_TIMER, pci_latency);
	pci_write_config_byte(dev, PCI_INTERRUPT_LINE, (u8)pci_irq);

#ifdef RAPCI_DEBUG
	pci_read_config_byte(dev, PCI_CACHE_LINE_SIZE, &pci_cache_line);
	printk("PCI_CACHE_LINE_SIZE = %d\n", (pci_cache_line << 2));

	pci_read_config_byte(dev, PCI_LATENCY_TIMER, &pci_latency);
	printk("PCI_LATENCY_TIMER = %d\n", pci_latency);

	for (i = 0; i < 2; i++) {
		res = (struct resource*)&dev->resource[i];
		printk("res[%d]->start = %x\n", i, res->start);
		printk("res[%d]->end = %x\n", i, res->end);
	}
#endif

	return pci_irq;
}

#if defined (CONFIG_RALINK_MT7621)
static void set_pcie_phy(u32 *addr, int start_b, int bits, int val)
{
	*(volatile u32 *)(addr) &= ~(((1<<bits) - 1)<<start_b);
	*(volatile u32 *)(addr) |= val << start_b;
}

static void bypass_pipe_rst(void)
{
#if defined (CONFIG_PCIE_PORT0)
	/* PCIe Port 0 */
	set_pcie_phy((u32 *)(RALINK_PCIEPHY_P0P1_CTL_OFFSET + 0x02c), 12, 1, 0x01);	// rg_pe1_pipe_rst_b
	set_pcie_phy((u32 *)(RALINK_PCIEPHY_P0P1_CTL_OFFSET + 0x02c),  4, 1, 0x01);	// rg_pe1_pipe_cmd_frc[4]
#endif
#if defined (CONFIG_PCIE_PORT1)
	/* PCIe Port 1 */
	set_pcie_phy((u32 *)(RALINK_PCIEPHY_P0P1_CTL_OFFSET + 0x12c), 12, 1, 0x01);	// rg_pe1_pipe_rst_b
	set_pcie_phy((u32 *)(RALINK_PCIEPHY_P0P1_CTL_OFFSET + 0x12c),  4, 1, 0x01);	// rg_pe1_pipe_cmd_frc[4]
#endif
#if defined (CONFIG_PCIE_PORT2)
	/* PCIe Port 2 */
	set_pcie_phy((u32 *)(RALINK_PCIEPHY_P2_CTL_OFFSET + 0x02c), 12, 1, 0x01);	// rg_pe1_pipe_rst_b
	set_pcie_phy((u32 *)(RALINK_PCIEPHY_P2_CTL_OFFSET + 0x02c),  4, 1, 0x01);	// rg_pe1_pipe_cmd_frc[4]
#endif
}

static void set_phy_for_ssc(void)
{
	unsigned long reg = (*(volatile u32 *)(RALINK_SYSCTL_BASE + 0x10));

	reg = (reg >> 6) & 0x7;
#if defined (CONFIG_PCIE_PORT0) || defined (CONFIG_PCIE_PORT1)
	/* Set PCIe Port0 & Port1 PHY to disable SSC */
	/* Debug Xtal Type */
	set_pcie_phy((u32 *)(RALINK_PCIEPHY_P0P1_CTL_OFFSET + 0x400),  8, 1, 0x01);	// rg_pe1_frc_h_xtal_type
	set_pcie_phy((u32 *)(RALINK_PCIEPHY_P0P1_CTL_OFFSET + 0x400),  9, 2, 0x00);	// rg_pe1_h_xtal_type
	set_pcie_phy((u32 *)(RALINK_PCIEPHY_P0P1_CTL_OFFSET + 0x000),  4, 1, 0x01);	// rg_pe1_frc_phy_en               //Force Port 0 enable control
	set_pcie_phy((u32 *)(RALINK_PCIEPHY_P0P1_CTL_OFFSET + 0x100),  4, 1, 0x01);	// rg_pe1_frc_phy_en               //Force Port 1 enable control
	set_pcie_phy((u32 *)(RALINK_PCIEPHY_P0P1_CTL_OFFSET + 0x000),  5, 1, 0x00);	// rg_pe1_phy_en                   //Port 0 disable
	set_pcie_phy((u32 *)(RALINK_PCIEPHY_P0P1_CTL_OFFSET + 0x100),  5, 1, 0x00);	// rg_pe1_phy_en                   //Port 1 disable
	if(reg <= 5 && reg >= 3) { 	// 40MHz Xtal
		set_pcie_phy((u32 *)(RALINK_PCIEPHY_P0P1_CTL_OFFSET + 0x490),  6, 2, 0x01);	// RG_PE1_H_PLL_PREDIV             //Pre-divider ratio (for host mode)
		printk("***** Xtal 40MHz *****\n");
	} else {			// 25MHz | 20MHz Xtal
		set_pcie_phy((u32 *)(RALINK_PCIEPHY_P0P1_CTL_OFFSET + 0x490),  6, 2, 0x00);	// RG_PE1_H_PLL_PREDIV             //Pre-divider ratio (for host mode)
		if (reg >= 6) { 	
			printk("***** Xtal 25MHz *****\n");
			set_pcie_phy((u32 *)(RALINK_PCIEPHY_P0P1_CTL_OFFSET + 0x4bc),  4, 2, 0x01);	// RG_PE1_H_PLL_FBKSEL             //Feedback clock select
			set_pcie_phy((u32 *)(RALINK_PCIEPHY_P0P1_CTL_OFFSET + 0x49c),  0,31, 0x18000000);	// RG_PE1_H_LCDDS_PCW_NCPO         //DDS NCPO PCW (for host mode)
			set_pcie_phy((u32 *)(RALINK_PCIEPHY_P0P1_CTL_OFFSET + 0x4a4),  0,16, 0x18d);	// RG_PE1_H_LCDDS_SSC_PRD          //DDS SSC dither period control
			set_pcie_phy((u32 *)(RALINK_PCIEPHY_P0P1_CTL_OFFSET + 0x4a8),  0,12, 0x4a);	// RG_PE1_H_LCDDS_SSC_DELTA        //DDS SSC dither amplitude control
			set_pcie_phy((u32 *)(RALINK_PCIEPHY_P0P1_CTL_OFFSET + 0x4a8), 16,12, 0x4a);	// RG_PE1_H_LCDDS_SSC_DELTA1       //DDS SSC dither amplitude control for initial
		} else {
			printk("***** Xtal 20MHz *****\n");
		}
	}
	set_pcie_phy((u32 *)(RALINK_PCIEPHY_P0P1_CTL_OFFSET + 0x4a0),  5, 1, 0x01);	// RG_PE1_LCDDS_CLK_PH_INV         //DDS clock inversion
	set_pcie_phy((u32 *)(RALINK_PCIEPHY_P0P1_CTL_OFFSET + 0x490), 22, 2, 0x02);	// RG_PE1_H_PLL_BC                 
	set_pcie_phy((u32 *)(RALINK_PCIEPHY_P0P1_CTL_OFFSET + 0x490), 18, 4, 0x06);	// RG_PE1_H_PLL_BP                 
	set_pcie_phy((u32 *)(RALINK_PCIEPHY_P0P1_CTL_OFFSET + 0x490), 12, 4, 0x02);	// RG_PE1_H_PLL_IR                 
	set_pcie_phy((u32 *)(RALINK_PCIEPHY_P0P1_CTL_OFFSET + 0x490),  8, 4, 0x01);	// RG_PE1_H_PLL_IC                 
	set_pcie_phy((u32 *)(RALINK_PCIEPHY_P0P1_CTL_OFFSET + 0x4ac), 16, 3, 0x00);	// RG_PE1_H_PLL_BR                 
	set_pcie_phy((u32 *)(RALINK_PCIEPHY_P0P1_CTL_OFFSET + 0x490),  1, 3, 0x02);	// RG_PE1_PLL_DIVEN                
	if(reg <= 5 && reg >= 3) { 	// 40MHz Xtal
		set_pcie_phy((u32 *)(RALINK_PCIEPHY_P0P1_CTL_OFFSET + 0x414),  6, 2, 0x01);	// rg_pe1_mstckdiv		//value of da_pe1_mstckdiv when force mode enable
		set_pcie_phy((u32 *)(RALINK_PCIEPHY_P0P1_CTL_OFFSET + 0x414),  5, 1, 0x01);	// rg_pe1_frc_mstckdiv          //force mode enable of da_pe1_mstckdiv      
	}
	set_pcie_phy((u32 *)(RALINK_PCIEPHY_P0P1_CTL_OFFSET + 0x040), 17, 4, 0x07);	// rg_pe1_crtmsel                   //value of da[x]_pe1_crtmsel when force mode enable for Port 0
	set_pcie_phy((u32 *)(RALINK_PCIEPHY_P0P1_CTL_OFFSET + 0x040), 16, 1, 0x01);	// rg_pe1_frc_crtmsel               //force mode enable of da[x]_pe1_crtmsel for Port 0
	set_pcie_phy((u32 *)(RALINK_PCIEPHY_P0P1_CTL_OFFSET + 0x140), 17, 4, 0x07);	// rg_pe1_crtmsel                   //value of da[x]_pe1_crtmsel when force mode enable for Port 1
	set_pcie_phy((u32 *)(RALINK_PCIEPHY_P0P1_CTL_OFFSET + 0x140), 16, 1, 0x01);	// rg_pe1_frc_crtmsel               //force mode enable of da[x]_pe1_crtmsel for Port 1
	/* Enable PHY and disable force mode */
	set_pcie_phy((u32 *)(RALINK_PCIEPHY_P0P1_CTL_OFFSET + 0x000),  5, 1, 0x01);	// rg_pe1_phy_en                   //Port 0 enable
	set_pcie_phy((u32 *)(RALINK_PCIEPHY_P0P1_CTL_OFFSET + 0x100),  5, 1, 0x01);	// rg_pe1_phy_en                   //Port 1 enable
	set_pcie_phy((u32 *)(RALINK_PCIEPHY_P0P1_CTL_OFFSET + 0x000),  4, 1, 0x00);	// rg_pe1_frc_phy_en               //Force Port 0 disable control
	set_pcie_phy((u32 *)(RALINK_PCIEPHY_P0P1_CTL_OFFSET + 0x100),  4, 1, 0x00);	// rg_pe1_frc_phy_en               //Force Port 1 disable control
#endif
#if defined (CONFIG_PCIE_PORT2)
	/* Set PCIe Port2 PHY to disable SSC */
	/* Debug Xtal Type */
	set_pcie_phy((u32 *)(RALINK_PCIEPHY_P2_CTL_OFFSET + 0x400),  8, 1, 0x01);	// rg_pe1_frc_h_xtal_type
	set_pcie_phy((u32 *)(RALINK_PCIEPHY_P2_CTL_OFFSET + 0x400),  9, 2, 0x00);	// rg_pe1_h_xtal_type
	set_pcie_phy((u32 *)(RALINK_PCIEPHY_P2_CTL_OFFSET + 0x000),  4, 1, 0x01);	// rg_pe1_frc_phy_en               //Force Port 0 enable control
	set_pcie_phy((u32 *)(RALINK_PCIEPHY_P2_CTL_OFFSET + 0x000),  5, 1, 0x00);	// rg_pe1_phy_en                   //Port 0 disable
	if(reg <= 5 && reg >= 3) { 	// 40MHz Xtal
		set_pcie_phy((u32 *)(RALINK_PCIEPHY_P2_CTL_OFFSET + 0x490),  6, 2, 0x01);	// RG_PE1_H_PLL_PREDIV             //Pre-divider ratio (for host mode)
	} else {			// 25MHz | 20MHz Xtal
		set_pcie_phy((u32 *)(RALINK_PCIEPHY_P2_CTL_OFFSET + 0x490),  6, 2, 0x00);	// RG_PE1_H_PLL_PREDIV             //Pre-divider ratio (for host mode)
		if (reg >= 6) { 	// 25MHz Xtal
			set_pcie_phy((u32 *)(RALINK_PCIEPHY_P2_CTL_OFFSET + 0x4bc),  4, 2, 0x01);	// RG_PE1_H_PLL_FBKSEL             //Feedback clock select
			set_pcie_phy((u32 *)(RALINK_PCIEPHY_P2_CTL_OFFSET + 0x49c),  0,31, 0x18000000);	// RG_PE1_H_LCDDS_PCW_NCPO         //DDS NCPO PCW (for host mode)
			set_pcie_phy((u32 *)(RALINK_PCIEPHY_P2_CTL_OFFSET + 0x4a4),  0,16, 0x18d);	// RG_PE1_H_LCDDS_SSC_PRD          //DDS SSC dither period control
			set_pcie_phy((u32 *)(RALINK_PCIEPHY_P2_CTL_OFFSET + 0x4a8),  0,12, 0x4a);	// RG_PE1_H_LCDDS_SSC_DELTA        //DDS SSC dither amplitude control
			set_pcie_phy((u32 *)(RALINK_PCIEPHY_P2_CTL_OFFSET + 0x4a8), 16,12, 0x4a);	// RG_PE1_H_LCDDS_SSC_DELTA1       //DDS SSC dither amplitude control for initial
		}
	}
	set_pcie_phy((u32 *)(RALINK_PCIEPHY_P2_CTL_OFFSET + 0x4a0),  5, 1, 0x01);	// RG_PE1_LCDDS_CLK_PH_INV         //DDS clock inversion
	set_pcie_phy((u32 *)(RALINK_PCIEPHY_P2_CTL_OFFSET + 0x490), 22, 2, 0x02);	// RG_PE1_H_PLL_BC                 
	set_pcie_phy((u32 *)(RALINK_PCIEPHY_P2_CTL_OFFSET + 0x490), 18, 4, 0x06);	// RG_PE1_H_PLL_BP                 
	set_pcie_phy((u32 *)(RALINK_PCIEPHY_P2_CTL_OFFSET + 0x490), 12, 4, 0x02);	// RG_PE1_H_PLL_IR                 
	set_pcie_phy((u32 *)(RALINK_PCIEPHY_P2_CTL_OFFSET + 0x490),  8, 4, 0x01);	// RG_PE1_H_PLL_IC                 
	set_pcie_phy((u32 *)(RALINK_PCIEPHY_P2_CTL_OFFSET + 0x4ac), 16, 3, 0x00);	// RG_PE1_H_PLL_BR                 
	set_pcie_phy((u32 *)(RALINK_PCIEPHY_P2_CTL_OFFSET + 0x490),  1, 3, 0x02);	// RG_PE1_PLL_DIVEN                
	if(reg <= 5 && reg >= 3) { 	// 40MHz Xtal
		set_pcie_phy((u32 *)(RALINK_PCIEPHY_P2_CTL_OFFSET + 0x414),  6, 2, 0x01);	// rg_pe1_mstckdiv		//value of da_pe1_mstckdiv when force mode enable
		set_pcie_phy((u32 *)(RALINK_PCIEPHY_P2_CTL_OFFSET + 0x414),  5, 1, 0x01);	// rg_pe1_frc_mstckdiv          //force mode enable of da_pe1_mstckdiv      
	}
	set_pcie_phy((u32 *)(RALINK_PCIEPHY_P2_CTL_OFFSET + 0x040), 17, 4, 0x07);	// rg_pe1_crtmsel                   //value of da[x]_pe1_crtmsel when force mode enable for Port 0
	set_pcie_phy((u32 *)(RALINK_PCIEPHY_P2_CTL_OFFSET + 0x040), 16, 1, 0x01);	// rg_pe1_frc_crtmsel               //force mode enable of da[x]_pe1_crtmsel for Port 0
	/* Enable PHY and disable force mode */
	set_pcie_phy((u32 *)(RALINK_PCIEPHY_P2_CTL_OFFSET + 0x000),  5, 1, 0x01);	// rg_pe1_phy_en                   //Port 0 enable
	set_pcie_phy((u32 *)(RALINK_PCIEPHY_P2_CTL_OFFSET + 0x000),  4, 1, 0x00);	// rg_pe1_frc_phy_en               //Force Port 0 disable control
#endif
}
#endif

int __init init_ralink_pci(void)
{
	u32 val = 0;
#if defined (CONFIG_RALINK_RT3883)
	int pcie_disable = 0;
#endif

	PCIBIOS_MIN_IO = 0;
	PCIBIOS_MIN_MEM = 0;

#if defined (CONFIG_RALINK_MT7621)
	pcie_link_status = 0;
	ASSERT_SYSRST_PCIE(RALINK_PCIE0_RST | RALINK_PCIE1_RST | RALINK_PCIE2_RST);
#if defined (GPIO_PERST)
	RALINK_GPIOMODE &= ~(0x3<<PCIE_SHARE_PIN_SW | 0x3<<UARTL3_SHARE_PIN_SW);
	RALINK_GPIOMODE |=  (0x1<<PCIE_SHARE_PIN_SW | 0x1<<UARTL3_SHARE_PIN_SW);
	mdelay(100);
#if defined (CONFIG_PCIE_PORT0)
	val = 0x1<<GPIO_PCIE_PORT0;
#endif
#if defined (CONFIG_PCIE_PORT1)
	val |= 0x1<<GPIO_PCIE_PORT1;
#endif
#if defined (CONFIG_PCIE_PORT2)
	val |= 0x1<<GPIO_PCIE_PORT2;
#endif
	RALINK_GPIO_CTRL0 |= val;			// switch to output mode
	mdelay(100);
	RALINK_GPIO_DATA0 &= ~(val);			// clear DATA
	mdelay(100);
#else /* !defined (GPIO_PERST) */
	RALINK_GPIOMODE &= ~(0x3<<PCIE_SHARE_PIN_SW);
#endif

#if defined (CONFIG_PCIE_PORT0)
	val  = RALINK_PCIE0_RST;
#endif
#if defined (CONFIG_PCIE_PORT1)
	val |= RALINK_PCIE1_RST;
#endif
#if defined (CONFIG_PCIE_PORT2)
	val |= RALINK_PCIE2_RST;
#endif
	DEASSERT_SYSRST_PCIE(val);

	printk("PCIe PHY initialize\n");

	if ((ralink_asic_rev_id & 0xFFFF) == 0x0101) // MT7621 E2
		bypass_pipe_rst();
	set_phy_for_ssc();

#if defined (GPIO_PERST)
#if defined (CONFIG_PCIE_PORT0)
	val  = 0x1<<GPIO_PCIE_PORT0;
#endif
#if defined (CONFIG_PCIE_PORT1)
	val |= 0x1<<GPIO_PCIE_PORT1;
#endif
#if defined (CONFIG_PCIE_PORT2)
	val |= 0x1<<GPIO_PCIE_PORT2;
#endif
	RALINK_GPIO_DATA0 |= val;
#else /* !defined (GPIO_PERST) */
	RALINK_PCI_PCICFG_ADDR &= ~(1<<1);		// release PCIRST
#endif

#elif defined (CONFIG_RALINK_MT7628)
	RALINK_GPIOMODE &= ~(0x1<<9);			// PCIe RESET GPIO mode
	RALINK_RSTCTRL &= ~RALINK_PCIE0_RST;
	RALINK_CLKCFG1 |=  RALINK_PCIE0_CLK_EN;

	mdelay(100);

	/* set N_FTS */
	val = 0;
	config_access(PCI_ACCESS_READ_4, 0, 0x0, 0, 0x70c, &val);
	val &= ~(0xff)<<8;
	val |= 0x50<<8;
	config_access(PCI_ACCESS_WRITE_4, 0, 0x0, 0, 0x70c, &val);

	RALINK_PCI_PCICFG_ADDR &= ~(1<<1);		// release PCIRST
#elif defined (CONFIG_RALINK_MT7620)
	RALINK_GPIOMODE &= ~(0x3<<16);			// PERST_GPIO_MODE = 2'b00
	RALINK_SYSCFG1 |=  RALINK_PCIE_RC_MODE_EN;	// PCIe in RC mode
	RALINK_RSTCTRL &= ~RALINK_PCIE0_RST;
	RALINK_CLKCFG1 |=  RALINK_PCIE0_CLK_EN;

	mdelay(100);

	if ( !(PPLL_CFG1 & (1<<23)) ) {
		printk("MT7620 PPLL unlock, cannot enable PCIe!\n");
		/* for power saving */
		RALINK_RSTCTRL |=  RALINK_PCIE0_RST;
		RALINK_CLKCFG1 &= ~RALINK_PCIE0_CLK_EN;
		return 0;
	}

	PPLL_DRV |=  LC_CKDRVPD;			// PCIe clock driver power ON
	PPLL_DRV &= ~LC_CKDRVOHZ;			// Reference PCIe Output clock mode enable
	PPLL_DRV &= ~LC_CKDRVHZ;			// PCIe PHY clock enable
	PPLL_DRV |=  PDRV_SW_SET;			// PDRV SW Set

	mdelay(50);

	RALINK_PCI_PCICFG_ADDR &= ~(1<<1);		// release PCIRST
#elif defined (CONFIG_RALINK_RT3883)

#if defined (CONFIG_PCIE_ONLY) || defined (CONFIG_PCIE_PCI_CONCURRENT)
	RALINK_RSTCTRL |= RALINK_PCIE_RST;
	RALINK_SYSCFG1 &= ~(0x30);
	RALINK_SYSCFG1 |= (2 << 4);
	RALINK_PCIE_CLK_GEN &= 0x7fffffff;
	RALINK_PCIE_CLK_GEN1 &= 0x80ffffff;
	RALINK_PCIE_CLK_GEN1 |= (0xa << 24);
	RALINK_PCIE_CLK_GEN |= 0x80000000;
	mdelay(50);
	RALINK_RSTCTRL &= ~RALINK_PCIE_RST;
#endif
#if defined (CONFIG_PCI_ONLY) || defined (CONFIG_PCIE_PCI_CONCURRENT)
	RALINK_GPIOMODE = (((RALINK_GPIOMODE) & ~(0x1800)) | PCI_SLOTx2);	// enable PCI slot 1, disable PCI slot 2
#endif
	RALINK_SYSCFG1 |= (RALINK_PCI_HOST_MODE_EN | RALINK_PCIE_RC_MODE_EN);	// PCI in host mode, PCIe in RC mode
#if defined (CONFIG_PCI_ONLY)
	RALINK_RSTCTRL |=  RALINK_PCIE_RST;
	RALINK_CLKCFG1 &= ~RALINK_PCIE_CLK_EN;
#elif defined (CONFIG_PCIE_ONLY)
	RALINK_RSTCTRL |=  RALINK_PCI_RST;
	RALINK_CLKCFG1 &= ~RALINK_PCI_CLK_EN;
#endif
	mdelay(200);

#if defined (CONFIG_PCIE_ONLY)
	RALINK_PCI_PCICFG_ADDR = 0;		// virtual P2P bridge DEVNUM = 0, release PCIRST
#else
	RALINK_PCI_PCICFG_ADDR = (1 << 16);	// virtual P2P bridge DEVNUM = 1, release PCIRST
#endif

#endif

	mdelay(500);

#if defined (CONFIG_RALINK_MT7621)
#if defined (CONFIG_PCIE_PORT0)
	if ((RALINK_PCI0_STATUS & 0x1) == 0) {
		ASSERT_SYSRST_PCIE(RALINK_PCIE0_RST);
		RALINK_CLKCFG1 &= ~RALINK_PCIE0_CLK_EN;
		printk("%s: no card, disable it (RST&CLK)\n", "PCIe0");
	} else {
		pcie_link_status |= 1<<0;
		RALINK_PCI_PCIMSK_ADDR |= (1<<20); // enable pcie1 interrupt
	}
#endif
#if defined (CONFIG_PCIE_PORT1)
	if ((RALINK_PCI1_STATUS & 0x1) == 0) {
		ASSERT_SYSRST_PCIE(RALINK_PCIE1_RST);
		RALINK_CLKCFG1 &= ~RALINK_PCIE1_CLK_EN;
		printk("%s: no card, disable it (RST&CLK)\n", "PCIe1");
	} else {
		pcie_link_status |= 1<<1;
		RALINK_PCI_PCIMSK_ADDR |= (1<<21); // enable pcie1 interrupt
	}
#endif
#if defined (CONFIG_PCIE_PORT2)
	if ((RALINK_PCI2_STATUS & 0x1) == 0) {
		ASSERT_SYSRST_PCIE(RALINK_PCIE2_RST);
		RALINK_CLKCFG1 &= ~RALINK_PCIE2_CLK_EN;
		printk("%s: no card, disable it (RST&CLK)\n", "PCIe2");
	} else {
		pcie_link_status |= 1<<2;
		RALINK_PCI_PCIMSK_ADDR |= (1<<22); // enable pcie2 interrupt
	}
#endif
	if (pcie_link_status == 0)
		return 0;

	printk("*** Configure Device number setting of Virtual PCI-PCI bridge ***\n");
/*
pcie(2/1/0) link status	pcie2_num	pcie1_num	pcie0_num
3'b000			x		x		x
3'b001			x		x		0
3'b010			x		0		x
3'b011			x		1		0
3'b100			0		x		x
3'b101			1		x		0
3'b110			1		0		x
3'b111			2		1		0
*/
	switch (pcie_link_status) {
	case 2:
		RALINK_PCI_PCICFG_ADDR &= ~0x00ff0000;
		RALINK_PCI_PCICFG_ADDR |= 0x1 << 16;	//port0
		RALINK_PCI_PCICFG_ADDR |= 0x0 << 20;	//port1
		break;
	case 4:
		RALINK_PCI_PCICFG_ADDR &= ~0x0fff0000;
		RALINK_PCI_PCICFG_ADDR |= 0x1 << 16;	//port0
		RALINK_PCI_PCICFG_ADDR |= 0x2 << 20;	//port1
		RALINK_PCI_PCICFG_ADDR |= 0x0 << 24;	//port2
		break;
	case 5:
		RALINK_PCI_PCICFG_ADDR &= ~0x0fff0000;
		RALINK_PCI_PCICFG_ADDR |= 0x0 << 16;	//port0
		RALINK_PCI_PCICFG_ADDR |= 0x2 << 20;	//port1
		RALINK_PCI_PCICFG_ADDR |= 0x1 << 24;	//port2
		break;
	case 6:
		RALINK_PCI_PCICFG_ADDR &= ~0x0fff0000;
		RALINK_PCI_PCICFG_ADDR |= 0x2 << 16;	//port0
		RALINK_PCI_PCICFG_ADDR |= 0x0 << 20;	//port1
		RALINK_PCI_PCICFG_ADDR |= 0x1 << 24;	//port2
		break;
	}
	printk(" -> %x\n", RALINK_PCI_PCICFG_ADDR);
#elif defined (CONFIG_RALINK_MT7628)
	if ((RALINK_PCI0_STATUS & 0x1) == 0) {
		RALINK_RSTCTRL |= RALINK_PCIE0_RST;
		RALINK_CLKCFG1 &= ~RALINK_PCIE0_CLK_EN;
		printk("%s: no card, disable it (RST&CLK)\n", "PCIe0");
		return 0;
	}
#elif defined (CONFIG_RALINK_MT7620)
	if ((RALINK_PCI0_STATUS & 0x1) == 0) {
		RALINK_RSTCTRL |=  RALINK_PCIE0_RST;
		RALINK_CLKCFG1 &= ~RALINK_PCIE0_CLK_EN;
		PPLL_DRV &= ~LC_CKDRVPD;
		PPLL_DRV |=  PDRV_SW_SET;
		printk("%s: no card, disable it (RST&CLK)\n", "PCIe0");
		return 0;
	}
#elif defined (CONFIG_RALINK_RT3883)
#if defined (CONFIG_PCIE_ONLY) || defined (CONFIG_PCIE_PCI_CONCURRENT)
	if ((RALINK_PCI1_STATUS & 0x1) == 0) {
		RALINK_RSTCTRL |= RALINK_PCIE_RST;
		RALINK_CLKCFG1 &= ~RALINK_PCIE_CLK_EN;
		//cgrstb, cgpdb, pexdrven0, pexdrven1, cgpllrstb, cgpllpdb, pexclken
		RALINK_PCIE_CLK_GEN &= 0x0fff3f7f;
		pcie_disable = 1;
		printk("%s: no card, disable it (RST&CLK)\n", "PCIe");
#if defined (CONFIG_PCIE_ONLY)
		return 0;
#endif
	}
#endif
	RALINK_PCI_ARBCTL = 0x79;
#endif

	RALINK_PCI_MEMBASE = 0xffffffff;			// valid for host mode only
	RALINK_PCI_IOBASE = RALINK_PCI_IO_MAP_BASE;		// valid for host mode only

#if defined(CONFIG_RALINK_MT7621)
#if defined (CONFIG_PCIE_PORT0)
	// PCIe0
	if ((pcie_link_status & 0x1) != 0) {
		RALINK_PCI0_IMBASEBAR0_ADDR = MEMORY_BASE;
		RALINK_PCI0_BAR0SETUP_ADDR  = 0x7FFF0001;	// open 7FFF:2G; ENABLE
		RALINK_PCI0_CLASS           = 0x06040001;
	}
#endif
#if defined (CONFIG_PCIE_PORT1)
	// PCIe1
	if ((pcie_link_status & 0x2) != 0) {
		RALINK_PCI1_IMBASEBAR0_ADDR = MEMORY_BASE;
		RALINK_PCI1_BAR0SETUP_ADDR  = 0x7FFF0001;	// open 7FFF:2G; ENABLE
		RALINK_PCI1_CLASS           = 0x06040001;
	}
#endif
#if defined (CONFIG_PCIE_PORT2)
	// PCIe2
	if ((pcie_link_status & 0x4) != 0) {
		RALINK_PCI2_IMBASEBAR0_ADDR = MEMORY_BASE;
		RALINK_PCI2_BAR0SETUP_ADDR  = 0x7FFF0001;	// open 7FFF:2G; ENABLE
		RALINK_PCI2_CLASS           = 0x06040001;
	}
#endif
#elif defined (CONFIG_RALINK_MT7620) || defined (CONFIG_RALINK_MT7628)
	//PCIe0
	RALINK_PCI0_IMBASEBAR0_ADDR = MEMORY_BASE;
	RALINK_PCI0_BAR0SETUP_ADDR  = 0x7FFF0001;		// open 7FFF:2G; ENABLE
//	RALINK_PCI0_ID              = 0x08021814;
	RALINK_PCI0_CLASS           = 0x06040001;
//	RALINK_PCI0_SUBID           = 0x63521814;
	RALINK_PCI_PCIMSK_ADDR      = 0x00100000;		// enable PCIe0 interrupt
#elif defined (CONFIG_RALINK_RT3883)
	//PCI
	RALINK_PCI0_IMBASEBAR0_ADDR = MEMORY_BASE;
	RALINK_PCI0_BAR0SETUP_ADDR  = BAR_MASK;
	RALINK_PCI0_ID              = 0x08021814;
	RALINK_PCI0_CLASS           = 0x00800001;
	RALINK_PCI0_SUBID           = 0x38831814;
#if defined (CONFIG_PCI_ONLY) || defined (CONFIG_PCIE_PCI_CONCURRENT)
	RALINK_PCI0_BAR0SETUP_ADDR  = (BAR_MASK | 1);		// enable PCI BAR0
#endif
	//PCIe
	RALINK_PCI1_IMBASEBAR0_ADDR = MEMORY_BASE;
	RALINK_PCI1_BAR0SETUP_ADDR  = BAR_MASK;
	RALINK_PCI1_ID              = 0x08021814;
	RALINK_PCI1_CLASS           = 0x06040001;
	RALINK_PCI1_SUBID           = 0x38831814;
#if defined (CONFIG_PCIE_ONLY) || defined (CONFIG_PCIE_PCI_CONCURRENT)
	if (!pcie_disable)
		RALINK_PCI1_BAR0SETUP_ADDR  = (BAR_MASK | 1);	// enable PCIe BAR0
#endif
#if defined (CONFIG_PCIE_ONLY)
	RALINK_PCI_PCIMSK_ADDR      = 0x00100000;		// enable PCIe interrupt
#elif defined (CONFIG_PCI_ONLY)
	RALINK_PCI_PCIMSK_ADDR      = 0x000c0000;		// enable PCI interrupt
#elif defined (CONFIG_PCIE_PCI_CONCURRENT)
	RALINK_PCI_PCIMSK_ADDR      = (pcie_disable) ? 0x000c0000 : 0x001c0000;	// enable PCI/PCIe interrupt
#endif
#endif

	val = 0;

#if defined (CONFIG_RALINK_MT7621)
	switch (pcie_link_status)
	{
	case 7:
		config_access(PCI_ACCESS_READ_4, 0, 0x2, 0, PCI_COMMAND, &val);
		val |= PCI_COMMAND_MASTER;
		config_access(PCI_ACCESS_WRITE_4, 0, 0x2, 0, PCI_COMMAND, &val);
		
		config_access(PCI_ACCESS_READ_4, 0, 0x2, 0, 0x70c, &val);
		val &= ~(0xff)<<8;
		val |=  (0x50<<8);
		config_access(PCI_ACCESS_WRITE_4, 0, 0x2, 0, 0x70c, &val);
	case 3:
	case 5:
	case 6:
		config_access(PCI_ACCESS_READ_4, 0, 0x1, 0, PCI_COMMAND, &val);
		val |= PCI_COMMAND_MASTER;
		config_access(PCI_ACCESS_WRITE_4, 0, 0x1, 0, PCI_COMMAND, &val);
		
		config_access(PCI_ACCESS_READ_4, 0, 0x1, 0, 0x70c, &val);
		val &= ~(0xff)<<8;
		val |=  (0x50<<8);
		config_access(PCI_ACCESS_WRITE_4, 0, 0x1, 0, 0x70c, &val);
	default:
		config_access(PCI_ACCESS_READ_4, 0, 0x0, 0, PCI_COMMAND, &val);
		val |= PCI_COMMAND_MASTER;
		config_access(PCI_ACCESS_WRITE_4, 0, 0x0, 0, PCI_COMMAND, &val);
		
		config_access(PCI_ACCESS_READ_4, 0, 0x0, 0, 0x70c, &val);
		val &= ~(0xff)<<8;
		val |=  (0x50<<8);
		config_access(PCI_ACCESS_WRITE_4, 0, 0x0, 0, 0x70c, &val);
	}
#elif defined (CONFIG_RALINK_MT7620) || defined (CONFIG_RALINK_MT7628)
	// start P2P bridge PCIe0
	config_access(PCI_ACCESS_READ_4, 0, 0x0, 0, PCI_COMMAND, &val);
	val |= PCI_COMMAND_IO | PCI_COMMAND_MEMORY | PCI_COMMAND_MASTER;
	config_access(PCI_ACCESS_WRITE_4, 0, 0x0, 0, PCI_COMMAND, &val);
#elif defined (CONFIG_RALINK_RT3883)
#if defined (CONFIG_PCIE_PCI_CONCURRENT)
	// start virtual P2P bridge
	if (!pcie_disable) {
		config_access(PCI_ACCESS_READ_4, 0, 0x1, 0, PCI_COMMAND, &val);
		val |= PCI_COMMAND_IO | PCI_COMMAND_MEMORY | PCI_COMMAND_MASTER;
		config_access(PCI_ACCESS_WRITE_4, 0, 0x1, 0, PCI_COMMAND, &val);
	}
#endif
	// start PCI host bridge (or virtual P2P bridge for PCIe only)
	config_access(PCI_ACCESS_READ_4, 0, 0x0, 0, PCI_COMMAND, &val);
	val |= PCI_COMMAND_IO | PCI_COMMAND_MEMORY | PCI_COMMAND_MASTER;
	config_access(PCI_ACCESS_WRITE_4, 0, 0x0, 0, PCI_COMMAND, &val);
#endif

	ralink_pci_controller.io_map_base = mips_io_port_base;

	register_pci_controller(&ralink_pci_controller);

	return 0;
}

int pcibios_plat_dev_init(struct pci_dev *dev)
{
	return 0;
}

#if defined (CONFIG_RALINK_RT3883) || defined (CONFIG_RALINK_MT7620) || \
    defined (CONFIG_RALINK_MT7621) || defined (CONFIG_RALINK_MT7628)
arch_initcall(init_ralink_pci);
#else
#error Please disable CONFIG_PCI for unsupported SoC!
#endif

#endif /* CONFIG_PCI */
