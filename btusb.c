/*
 *
 *  Generic Bluetooth USB driver
 *
 *  Copyright (C) 2005-2008  Marcel Holtmann <marcel@holtmann.org>
 *
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 */

#include <linux/module.h>
#include <linux/usb.h>
#include <linux/firmware.h>
#include <asm/unaligned.h>

#include <net/bluetooth/bluetooth.h>
#include <net/bluetooth/hci_core.h>
#include <linux/version.h>

#define VERSION "0.7"

static bool disable_scofix;
static bool force_scofix;

static bool reset = 1;

static struct usb_driver btusb_driver;

#define BTUSB_IGNORE		0x01
#define BTUSB_DIGIANSWER	0x02
#define BTUSB_CSR		0x04
#define BTUSB_SNIFFER		0x08
#define BTUSB_BCM92035		0x10
#define BTUSB_BROKEN_ISOC	0x20
#define BTUSB_WRONG_SCO_MTU	0x40
#define BTUSB_ATH3012		0x80
#define BTUSB_INTEL		0x100
#define BTUSB_INTEL_BOOT	0x200
#define BTUSB_BCM_PATCHRAM	0x400
#define BTUSB_MARVELL		0x800
#define BTUSB_SWAVE		0x1000
#define BTUSB_INTEL_NEW		0x2000
#define BTUSB_AMP		0x4000
#define BTUSB_RTL8723A		0x8000
#define BTUSB_RTL8723B		0x10000

static const struct usb_device_id btusb_table[] = {
	/* Generic Bluetooth USB device */
	{ USB_DEVICE_INFO(0xe0, 0x01, 0x01) },
	{ USB_INTERFACE_INFO(0xe0, 0x01, 0x01) },

	/* Generic Bluetooth AMP device */
	{ USB_DEVICE_INFO(0xe0, 0x01, 0x04), .driver_info = BTUSB_AMP },

	/* Apple-specific (Broadcom) devices */
	{ USB_VENDOR_AND_INTERFACE_INFO(0x05ac, 0xff, 0x01, 0x01) },

	/* MediaTek MT76x0E */
	{ USB_DEVICE(0x0e8d, 0x763f) },

	/* Broadcom SoftSailing reporting vendor specific */
	{ USB_DEVICE(0x0a5c, 0x21e1) },

	/* Apple MacBookPro 7,1 */
	{ USB_DEVICE(0x05ac, 0x8213) },

	/* Apple iMac11,1 */
	{ USB_DEVICE(0x05ac, 0x8215) },

	/* Apple MacBookPro6,2 */
	{ USB_DEVICE(0x05ac, 0x8218) },

	/* Apple MacBookAir3,1, MacBookAir3,2 */
	{ USB_DEVICE(0x05ac, 0x821b) },

	/* Apple MacBookAir4,1 */
	{ USB_DEVICE(0x05ac, 0x821f) },

	/* Apple MacBookPro8,2 */
	{ USB_DEVICE(0x05ac, 0x821a) },

	/* Apple MacMini5,1 */
	{ USB_DEVICE(0x05ac, 0x8281) },

	/* AVM BlueFRITZ! USB v2.0 */
	{ USB_DEVICE(0x057c, 0x3800), .driver_info = BTUSB_SWAVE },

	/* Bluetooth Ultraport Module from IBM */
	{ USB_DEVICE(0x04bf, 0x030a) },

	/* ALPS Modules with non-standard id */
	{ USB_DEVICE(0x044e, 0x3001) },
	{ USB_DEVICE(0x044e, 0x3002) },

	/* Ericsson with non-standard id */
	{ USB_DEVICE(0x0bdb, 0x1002) },

	/* Canyon CN-BTU1 with HID interfaces */
	{ USB_DEVICE(0x0c10, 0x0000) },

	/* Broadcom BCM20702A0 */
	{ USB_DEVICE(0x0489, 0xe042) },
	{ USB_DEVICE(0x04ca, 0x2003) },
	{ USB_DEVICE(0x0b05, 0x17b5) },
	{ USB_DEVICE(0x0b05, 0x17cb) },
	{ USB_DEVICE(0x413c, 0x8197) },
	{ USB_DEVICE(0x13d3, 0x3404),
	  .driver_info = BTUSB_BCM_PATCHRAM },

	/* Broadcom BCM20702B0 (Dynex/Insignia) */
	{ USB_DEVICE(0x19ff, 0x0239), .driver_info = BTUSB_BCM_PATCHRAM },

	/* Foxconn - Hon Hai */
	{ USB_VENDOR_AND_INTERFACE_INFO(0x0489, 0xff, 0x01, 0x01),
	  .driver_info = BTUSB_BCM_PATCHRAM },

	/* Lite-On Technology - Broadcom based */
	{ USB_VENDOR_AND_INTERFACE_INFO(0x04ca, 0xff, 0x01, 0x01),
	  .driver_info = BTUSB_BCM_PATCHRAM },

	/* Broadcom devices with vendor specific id */
	{ USB_VENDOR_AND_INTERFACE_INFO(0x0a5c, 0xff, 0x01, 0x01),
	  .driver_info = BTUSB_BCM_PATCHRAM },

	/* ASUSTek Computer - Broadcom based */
	{ USB_VENDOR_AND_INTERFACE_INFO(0x0b05, 0xff, 0x01, 0x01),
	  .driver_info = BTUSB_BCM_PATCHRAM },

	/* Belkin F8065bf - Broadcom based */
	{ USB_VENDOR_AND_INTERFACE_INFO(0x050d, 0xff, 0x01, 0x01) },

	/* IMC Networks - Broadcom based */
	{ USB_VENDOR_AND_INTERFACE_INFO(0x13d3, 0xff, 0x01, 0x01) },

	/* Intel Bluetooth USB Bootloader (RAM module) */
	{ USB_DEVICE(0x8087, 0x0a5a),
	  .driver_info = BTUSB_INTEL_BOOT | BTUSB_BROKEN_ISOC },

	{ }	/* Terminating entry */
};

MODULE_DEVICE_TABLE(usb, btusb_table);

static const struct usb_device_id blacklist_table[] = {
	/* CSR BlueCore devices */
	{ USB_DEVICE(0x0a12, 0x0001), .driver_info = BTUSB_CSR },

	/* Broadcom BCM2033 without firmware */
	{ USB_DEVICE(0x0a5c, 0x2033), .driver_info = BTUSB_IGNORE },

	/* Atheros 3011 with sflash firmware */
	{ USB_DEVICE(0x0489, 0xe027), .driver_info = BTUSB_IGNORE },
	{ USB_DEVICE(0x0489, 0xe03d), .driver_info = BTUSB_IGNORE },
	{ USB_DEVICE(0x0930, 0x0215), .driver_info = BTUSB_IGNORE },
	{ USB_DEVICE(0x0cf3, 0x3002), .driver_info = BTUSB_IGNORE },
	{ USB_DEVICE(0x0cf3, 0xe019), .driver_info = BTUSB_IGNORE },
	{ USB_DEVICE(0x13d3, 0x3304), .driver_info = BTUSB_IGNORE },

	/* Atheros AR9285 Malbec with sflash firmware */
	{ USB_DEVICE(0x03f0, 0x311d), .driver_info = BTUSB_IGNORE },

	/* Atheros 3012 with sflash firmware */
	{ USB_DEVICE(0x0489, 0xe04d), .driver_info = BTUSB_ATH3012 },
	{ USB_DEVICE(0x0489, 0xe04e), .driver_info = BTUSB_ATH3012 },
	{ USB_DEVICE(0x0489, 0xe056), .driver_info = BTUSB_ATH3012 },
	{ USB_DEVICE(0x0489, 0xe057), .driver_info = BTUSB_ATH3012 },
	{ USB_DEVICE(0x0489, 0xe05f), .driver_info = BTUSB_ATH3012 },
	{ USB_DEVICE(0x0489, 0xe078), .driver_info = BTUSB_ATH3012 },
	{ USB_DEVICE(0x04c5, 0x1330), .driver_info = BTUSB_ATH3012 },
	{ USB_DEVICE(0x04ca, 0x3004), .driver_info = BTUSB_ATH3012 },
	{ USB_DEVICE(0x04ca, 0x3005), .driver_info = BTUSB_ATH3012 },
	{ USB_DEVICE(0x04ca, 0x3006), .driver_info = BTUSB_ATH3012 },
	{ USB_DEVICE(0x04ca, 0x3007), .driver_info = BTUSB_ATH3012 },
	{ USB_DEVICE(0x04ca, 0x3008), .driver_info = BTUSB_ATH3012 },
	{ USB_DEVICE(0x04ca, 0x300b), .driver_info = BTUSB_ATH3012 },
	{ USB_DEVICE(0x04ca, 0x3010), .driver_info = BTUSB_ATH3012 },
	{ USB_DEVICE(0x0930, 0x0219), .driver_info = BTUSB_ATH3012 },
	{ USB_DEVICE(0x0930, 0x0220), .driver_info = BTUSB_ATH3012 },
	{ USB_DEVICE(0x0930, 0x0227), .driver_info = BTUSB_ATH3012 },
	{ USB_DEVICE(0x0b05, 0x17d0), .driver_info = BTUSB_ATH3012 },
	{ USB_DEVICE(0x0cf3, 0x0036), .driver_info = BTUSB_ATH3012 },
	{ USB_DEVICE(0x0cf3, 0x3004), .driver_info = BTUSB_ATH3012 },
	{ USB_DEVICE(0x0cf3, 0x3008), .driver_info = BTUSB_ATH3012 },
	{ USB_DEVICE(0x0cf3, 0x311d), .driver_info = BTUSB_ATH3012 },
	{ USB_DEVICE(0x0cf3, 0x311e), .driver_info = BTUSB_ATH3012 },
	{ USB_DEVICE(0x0cf3, 0x311f), .driver_info = BTUSB_ATH3012 },
	{ USB_DEVICE(0x0cf3, 0x3121), .driver_info = BTUSB_ATH3012 },
	{ USB_DEVICE(0x0cf3, 0x817a), .driver_info = BTUSB_ATH3012 },
	{ USB_DEVICE(0x0cf3, 0xe003), .driver_info = BTUSB_ATH3012 },
	{ USB_DEVICE(0x0cf3, 0xe004), .driver_info = BTUSB_ATH3012 },
	{ USB_DEVICE(0x0cf3, 0xe005), .driver_info = BTUSB_ATH3012 },
	{ USB_DEVICE(0x13d3, 0x3362), .driver_info = BTUSB_ATH3012 },
	{ USB_DEVICE(0x13d3, 0x3375), .driver_info = BTUSB_ATH3012 },
	{ USB_DEVICE(0x13d3, 0x3393), .driver_info = BTUSB_ATH3012 },
	{ USB_DEVICE(0x13d3, 0x3402), .driver_info = BTUSB_ATH3012 },
	{ USB_DEVICE(0x13d3, 0x3408), .driver_info = BTUSB_ATH3012 },
	{ USB_DEVICE(0x13d3, 0x3423), .driver_info = BTUSB_ATH3012 },
	{ USB_DEVICE(0x13d3, 0x3432), .driver_info = BTUSB_ATH3012 },

	/* Atheros AR5BBU12 with sflash firmware */
	{ USB_DEVICE(0x0489, 0xe02c), .driver_info = BTUSB_IGNORE },

	/* Atheros AR5BBU12 with sflash firmware */
	{ USB_DEVICE(0x0489, 0xe036), .driver_info = BTUSB_ATH3012 },
	{ USB_DEVICE(0x0489, 0xe03c), .driver_info = BTUSB_ATH3012 },

	/* Broadcom BCM2035 */
	{ USB_DEVICE(0x0a5c, 0x2009), .driver_info = BTUSB_BCM92035 },
	{ USB_DEVICE(0x0a5c, 0x200a), .driver_info = BTUSB_WRONG_SCO_MTU },
	{ USB_DEVICE(0x0a5c, 0x2035), .driver_info = BTUSB_WRONG_SCO_MTU },

	/* Broadcom BCM2045 */
	{ USB_DEVICE(0x0a5c, 0x2039), .driver_info = BTUSB_WRONG_SCO_MTU },
	{ USB_DEVICE(0x0a5c, 0x2101), .driver_info = BTUSB_WRONG_SCO_MTU },

	/* IBM/Lenovo ThinkPad with Broadcom chip */
	{ USB_DEVICE(0x0a5c, 0x201e), .driver_info = BTUSB_WRONG_SCO_MTU },
	{ USB_DEVICE(0x0a5c, 0x2110), .driver_info = BTUSB_WRONG_SCO_MTU },

	/* HP laptop with Broadcom chip */
	{ USB_DEVICE(0x03f0, 0x171d), .driver_info = BTUSB_WRONG_SCO_MTU },

	/* Dell laptop with Broadcom chip */
	{ USB_DEVICE(0x413c, 0x8126), .driver_info = BTUSB_WRONG_SCO_MTU },

	/* Dell Wireless 370 and 410 devices */
	{ USB_DEVICE(0x413c, 0x8152), .driver_info = BTUSB_WRONG_SCO_MTU },
	{ USB_DEVICE(0x413c, 0x8156), .driver_info = BTUSB_WRONG_SCO_MTU },

	/* Belkin F8T012 and F8T013 devices */
	{ USB_DEVICE(0x050d, 0x0012), .driver_info = BTUSB_WRONG_SCO_MTU },
	{ USB_DEVICE(0x050d, 0x0013), .driver_info = BTUSB_WRONG_SCO_MTU },

	/* Asus WL-BTD202 device */
	{ USB_DEVICE(0x0b05, 0x1715), .driver_info = BTUSB_WRONG_SCO_MTU },

	/* Kensington Bluetooth USB adapter */
	{ USB_DEVICE(0x047d, 0x105e), .driver_info = BTUSB_WRONG_SCO_MTU },

	/* RTX Telecom based adapters with buggy SCO support */
	{ USB_DEVICE(0x0400, 0x0807), .driver_info = BTUSB_BROKEN_ISOC },
	{ USB_DEVICE(0x0400, 0x080a), .driver_info = BTUSB_BROKEN_ISOC },

	/* CONWISE Technology based adapters with buggy SCO support */
	{ USB_DEVICE(0x0e5e, 0x6622), .driver_info = BTUSB_BROKEN_ISOC },

	/* Roper Class 1 Bluetooth Dongle (Silicon Wave based) */
	{ USB_DEVICE(0x1300, 0x0001), .driver_info = BTUSB_SWAVE },

	/* Digianswer devices */
	{ USB_DEVICE(0x08fd, 0x0001), .driver_info = BTUSB_DIGIANSWER },
	{ USB_DEVICE(0x08fd, 0x0002), .driver_info = BTUSB_IGNORE },

	/* CSR BlueCore Bluetooth Sniffer */
	{ USB_DEVICE(0x0a12, 0x0002),
	  .driver_info = BTUSB_SNIFFER | BTUSB_BROKEN_ISOC },

	/* Frontline ComProbe Bluetooth Sniffer */
	{ USB_DEVICE(0x16d3, 0x0002),
	  .driver_info = BTUSB_SNIFFER | BTUSB_BROKEN_ISOC },

	/* Marvell Bluetooth devices */
	{ USB_DEVICE(0x1286, 0x2044), .driver_info = BTUSB_MARVELL },
	{ USB_DEVICE(0x1286, 0x2046), .driver_info = BTUSB_MARVELL },

	/* Intel Bluetooth devices */
	{ USB_DEVICE(0x8087, 0x07dc), .driver_info = BTUSB_INTEL },
	{ USB_DEVICE(0x8087, 0x0a2a), .driver_info = BTUSB_INTEL },
	{ USB_DEVICE(0x8087, 0x0a2b), .driver_info = BTUSB_INTEL_NEW },

	/* Other Intel Bluetooth devices */
	{ USB_VENDOR_AND_INTERFACE_INFO(0x8087, 0xe0, 0x01, 0x01),
	  .driver_info = BTUSB_IGNORE },

	/* Realtek 8723AE Bluetooth devices */
	{ USB_DEVICE(0x0930, 0x021d), .driver_info = BTUSB_RTL8723A },
	{ USB_DEVICE(0x0bda, 0x0723), .driver_info = BTUSB_RTL8723A },
	{ USB_DEVICE(0x0bda, 0x8723), .driver_info = BTUSB_RTL8723A },
	{ USB_DEVICE(0x13d3, 0x3394), .driver_info = BTUSB_RTL8723A },

	/* Realtek 8723AU Bluetooth devices */
	{ USB_DEVICE(0x0bda, 0x0724), .driver_info = BTUSB_RTL8723A },
	{ USB_DEVICE(0x0bda, 0x1724), .driver_info = BTUSB_RTL8723A },
	{ USB_DEVICE(0x0bda, 0x8725), .driver_info = BTUSB_RTL8723A },
	{ USB_DEVICE(0x0bda, 0x872a), .driver_info = BTUSB_RTL8723A },
	{ USB_DEVICE(0x0bda, 0x872b), .driver_info = BTUSB_RTL8723A },
	{ USB_DEVICE(0x0bda, 0xa724), .driver_info = BTUSB_RTL8723A },

	/* Realtek 8723BE Bluetooth devices */
	{ USB_DEVICE(0x0489, 0xe085), .driver_info = BTUSB_RTL8723B },
	{ USB_DEVICE(0x0489, 0xe08b), .driver_info = BTUSB_RTL8723B },
	{ USB_DEVICE(0x0930, 0x0222), .driver_info = BTUSB_RTL8723B },
	{ USB_DEVICE(0x0bda, 0xb001), .driver_info = BTUSB_RTL8723B },
	{ USB_DEVICE(0x0bda, 0xb002), .driver_info = BTUSB_RTL8723B },
	{ USB_DEVICE(0x0bda, 0xb003), .driver_info = BTUSB_RTL8723B },
	{ USB_DEVICE(0x0bda, 0xb004), .driver_info = BTUSB_RTL8723B },
	{ USB_DEVICE(0x0bda, 0xb005), .driver_info = BTUSB_RTL8723B },
	{ USB_DEVICE(0x0bda, 0xb723), .driver_info = BTUSB_RTL8723B },
	{ USB_DEVICE(0x0bda, 0xb728), .driver_info = BTUSB_RTL8723B },
	{ USB_DEVICE(0x0bda, 0xb72b), .driver_info = BTUSB_RTL8723B },
	{ USB_DEVICE(0x13d3, 0x3410), .driver_info = BTUSB_RTL8723B },
	{ USB_DEVICE(0x13d3, 0x3416), .driver_info = BTUSB_RTL8723B },
	{ USB_DEVICE(0x13d3, 0x3459), .driver_info = BTUSB_RTL8723B },

	/* Realtek 8723BU Bluetooth devices */
	{ USB_DEVICE(0x0bda, 0xb720), .driver_info = BTUSB_RTL8723B },
	{ USB_DEVICE(0x0bda, 0xb72a), .driver_info = BTUSB_RTL8723B },

	/* Realtek 8821AE Bluetooth devices */
	{ USB_DEVICE(0x0b05, 0x17dc), .driver_info = BTUSB_RTL8723B },
	{ USB_DEVICE(0x0bda, 0x0821), .driver_info = BTUSB_RTL8723B },
	{ USB_DEVICE(0x0bda, 0x8821), .driver_info = BTUSB_RTL8723B },
	{ USB_DEVICE(0x13d3, 0x3414), .driver_info = BTUSB_RTL8723B },
	{ USB_DEVICE(0x13d3, 0x3458), .driver_info = BTUSB_RTL8723B },
	{ USB_DEVICE(0x13d3, 0x3461), .driver_info = BTUSB_RTL8723B },
	{ USB_DEVICE(0x13d3, 0x3462), .driver_info = BTUSB_RTL8723B },

	/* Realtek 8821AU Bluetooth devices */
	{ USB_DEVICE(0x0bda, 0x0823), .driver_info = BTUSB_RTL8723B },

	/* Realtek 8761AU Bluetooth devices */
	{ USB_DEVICE(0x0bda, 0x8760), .driver_info = BTUSB_RTL8723B },
	{ USB_DEVICE(0x0bda, 0x8761), .driver_info = BTUSB_RTL8723B },
	{ USB_DEVICE(0x0bda, 0x8a60), .driver_info = BTUSB_RTL8723B },
	{ USB_DEVICE(0x0bda, 0xa761), .driver_info = BTUSB_RTL8723B },
	{ USB_DEVICE(0x0bda, 0xb761), .driver_info = BTUSB_RTL8723B },

	{ }	/* Terminating entry */
};

#define BTUSB_MAX_ISOC_FRAMES	10

#define BTUSB_INTR_RUNNING	0
#define BTUSB_BULK_RUNNING	1
#define BTUSB_ISOC_RUNNING	2
#define BTUSB_SUSPENDING	3
#define BTUSB_DID_ISO_RESUME	4
#define BTUSB_BOOTLOADER	5
#define BTUSB_DOWNLOADING	6
#define BTUSB_FIRMWARE_LOADED	7
#define BTUSB_FIRMWARE_FAILED	8
#define BTUSB_BOOTING		9
#define BTUSB_RESET_RESUME	10

struct btusb_data {
	struct hci_dev       *hdev;
	struct usb_device    *udev;
	struct usb_interface *intf;
	struct usb_interface *isoc;

	unsigned long flags;

	struct work_struct work;
	struct work_struct waker;

	struct usb_anchor deferred;
	struct usb_anchor tx_anchor;
	int tx_in_flight;
	spinlock_t txlock;

	struct usb_anchor intr_anchor;
	struct usb_anchor bulk_anchor;
	struct usb_anchor isoc_anchor;
	spinlock_t rxlock;

	struct sk_buff *evt_skb;
	struct sk_buff *acl_skb;
	struct sk_buff *sco_skb;

	struct usb_endpoint_descriptor *intr_ep;
	struct usb_endpoint_descriptor *bulk_tx_ep;
	struct usb_endpoint_descriptor *bulk_rx_ep;
	struct usb_endpoint_descriptor *isoc_tx_ep;
	struct usb_endpoint_descriptor *isoc_rx_ep;

	__u8 cmdreq_type;
	__u8 cmdreq;

	unsigned int sco_num;
	int isoc_altsetting;
	int suspend_count;

	int (*recv_event)(struct hci_dev *hdev, struct sk_buff *skb);
	int (*recv_bulk)(struct btusb_data *data, void *buffer, int count);
};

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(3, 18, 0))
static int btusb_wait_on_bit_timeout(void *word, int bit, unsigned long timeout,
				     unsigned mode)
{
	might_sleep();
	if (!test_bit(bit, word))
		return 0;
	return out_of_line_wait_on_bit_timeout(word, bit, bit_wait_timeout,
					       mode, timeout);
}
#endif

static inline void btusb_free_frags(struct btusb_data *data)
{
	unsigned long flags;

	spin_lock_irqsave(&data->rxlock, flags);

	kfree_skb(data->evt_skb);
	data->evt_skb = NULL;

	kfree_skb(data->acl_skb);
	data->acl_skb = NULL;

	kfree_skb(data->sco_skb);
	data->sco_skb = NULL;

	spin_unlock_irqrestore(&data->rxlock, flags);
}

static int btusb_recv_intr(struct btusb_data *data, void *buffer, int count)
{
	struct sk_buff *skb;
	int err = 0;

	spin_lock(&data->rxlock);
	skb = data->evt_skb;

	while (count) {
		int len;

		if (!skb) {
			skb = bt_skb_alloc(HCI_MAX_EVENT_SIZE, GFP_ATOMIC);
			if (!skb) {
				err = -ENOMEM;
				break;
			}

			bt_cb(skb)->pkt_type = HCI_EVENT_PKT;
			bt_cb(skb)->expect = HCI_EVENT_HDR_SIZE;
		}

		len = min_t(uint, bt_cb(skb)->expect, count);
		memcpy(skb_put(skb, len), buffer, len);

		count -= len;
		buffer += len;
		bt_cb(skb)->expect -= len;

		if (skb->len == HCI_EVENT_HDR_SIZE) {
			/* Complete event header */
			bt_cb(skb)->expect = hci_event_hdr(skb)->plen;

			if (skb_tailroom(skb) < bt_cb(skb)->expect) {
				kfree_skb(skb);
				skb = NULL;

				err = -EILSEQ;
				break;
			}
		}

		if (bt_cb(skb)->expect == 0) {
			/* Complete frame */
			data->recv_event(data->hdev, skb);
			skb = NULL;
		}
	}

	data->evt_skb = skb;
	spin_unlock(&data->rxlock);

	return err;
}

static int btusb_recv_bulk(struct btusb_data *data, void *buffer, int count)
{
	struct sk_buff *skb;
	int err = 0;

	spin_lock(&data->rxlock);
	skb = data->acl_skb;

	while (count) {
		int len;

		if (!skb) {
			skb = bt_skb_alloc(HCI_MAX_FRAME_SIZE, GFP_ATOMIC);
			if (!skb) {
				err = -ENOMEM;
				break;
			}

			bt_cb(skb)->pkt_type = HCI_ACLDATA_PKT;
			bt_cb(skb)->expect = HCI_ACL_HDR_SIZE;
		}

		len = min_t(uint, bt_cb(skb)->expect, count);
		memcpy(skb_put(skb, len), buffer, len);

		count -= len;
		buffer += len;
		bt_cb(skb)->expect -= len;

		if (skb->len == HCI_ACL_HDR_SIZE) {
			__le16 dlen = hci_acl_hdr(skb)->dlen;

			/* Complete ACL header */
			bt_cb(skb)->expect = __le16_to_cpu(dlen);

			if (skb_tailroom(skb) < bt_cb(skb)->expect) {
				kfree_skb(skb);
				skb = NULL;

				err = -EILSEQ;
				break;
			}
		}

		if (bt_cb(skb)->expect == 0) {
			/* Complete frame */
			hci_recv_frame(data->hdev, skb);
			skb = NULL;
		}
	}

	data->acl_skb = skb;
	spin_unlock(&data->rxlock);

	return err;
}

static int btusb_recv_isoc(struct btusb_data *data, void *buffer, int count)
{
	struct sk_buff *skb;
	int err = 0;

	spin_lock(&data->rxlock);
	skb = data->sco_skb;

	while (count) {
		int len;

		if (!skb) {
			skb = bt_skb_alloc(HCI_MAX_SCO_SIZE, GFP_ATOMIC);
			if (!skb) {
				err = -ENOMEM;
				break;
			}

			bt_cb(skb)->pkt_type = HCI_SCODATA_PKT;
			bt_cb(skb)->expect = HCI_SCO_HDR_SIZE;
		}

		len = min_t(uint, bt_cb(skb)->expect, count);
		memcpy(skb_put(skb, len), buffer, len);

		count -= len;
		buffer += len;
		bt_cb(skb)->expect -= len;

		if (skb->len == HCI_SCO_HDR_SIZE) {
			/* Complete SCO header */
			bt_cb(skb)->expect = hci_sco_hdr(skb)->dlen;

			if (skb_tailroom(skb) < bt_cb(skb)->expect) {
				kfree_skb(skb);
				skb = NULL;

				err = -EILSEQ;
				break;
			}
		}

		if (bt_cb(skb)->expect == 0) {
			/* Complete frame */
			hci_recv_frame(data->hdev, skb);
			skb = NULL;
		}
	}

	data->sco_skb = skb;
	spin_unlock(&data->rxlock);

	return err;
}

static void btusb_intr_complete(struct urb *urb)
{
	struct hci_dev *hdev = urb->context;
	struct btusb_data *data = hci_get_drvdata(hdev);
	int err;

	BT_DBG("%s urb %p status %d count %d", hdev->name, urb, urb->status,
	       urb->actual_length);

	if (!test_bit(HCI_RUNNING, &hdev->flags))
		return;

	if (urb->status == 0) {
		hdev->stat.byte_rx += urb->actual_length;

		if (btusb_recv_intr(data, urb->transfer_buffer,
				    urb->actual_length) < 0) {
			BT_ERR("%s corrupted event packet", hdev->name);
			hdev->stat.err_rx++;
		}
	} else if (urb->status == -ENOENT) {
		/* Avoid suspend failed when usb_kill_urb */
		return;
	}

	if (!test_bit(BTUSB_INTR_RUNNING, &data->flags))
		return;

	usb_mark_last_busy(data->udev);
	usb_anchor_urb(urb, &data->intr_anchor);

	err = usb_submit_urb(urb, GFP_ATOMIC);
	if (err < 0) {
		/* -EPERM: urb is being killed;
		 * -ENODEV: device got disconnected */
		if (err != -EPERM && err != -ENODEV)
			BT_ERR("%s urb %p failed to resubmit (%d)",
			       hdev->name, urb, -err);
		usb_unanchor_urb(urb);
	}
}

static int btusb_submit_intr_urb(struct hci_dev *hdev, gfp_t mem_flags)
{
	struct btusb_data *data = hci_get_drvdata(hdev);
	struct urb *urb;
	unsigned char *buf;
	unsigned int pipe;
	int err, size;

	BT_DBG("%s", hdev->name);

	if (!data->intr_ep)
		return -ENODEV;

	urb = usb_alloc_urb(0, mem_flags);
	if (!urb)
		return -ENOMEM;

	size = le16_to_cpu(data->intr_ep->wMaxPacketSize);

	buf = kmalloc(size, mem_flags);
	if (!buf) {
		usb_free_urb(urb);
		return -ENOMEM;
	}

	pipe = usb_rcvintpipe(data->udev, data->intr_ep->bEndpointAddress);

	usb_fill_int_urb(urb, data->udev, pipe, buf, size,
			 btusb_intr_complete, hdev, data->intr_ep->bInterval);

	urb->transfer_flags |= URB_FREE_BUFFER;

	usb_anchor_urb(urb, &data->intr_anchor);

	err = usb_submit_urb(urb, mem_flags);
	if (err < 0) {
		if (err != -EPERM && err != -ENODEV)
			BT_ERR("%s urb %p submission failed (%d)",
			       hdev->name, urb, -err);
		usb_unanchor_urb(urb);
	}

	usb_free_urb(urb);

	return err;
}

static void btusb_bulk_complete(struct urb *urb)
{
	struct hci_dev *hdev = urb->context;
	struct btusb_data *data = hci_get_drvdata(hdev);
	int err;

	BT_DBG("%s urb %p status %d count %d", hdev->name, urb, urb->status,
	       urb->actual_length);

	if (!test_bit(HCI_RUNNING, &hdev->flags))
		return;

	if (urb->status == 0) {
		hdev->stat.byte_rx += urb->actual_length;

		if (data->recv_bulk(data, urb->transfer_buffer,
				    urb->actual_length) < 0) {
			BT_ERR("%s corrupted ACL packet", hdev->name);
			hdev->stat.err_rx++;
		}
	} else if (urb->status == -ENOENT) {
		/* Avoid suspend failed when usb_kill_urb */
		return;
	}

	if (!test_bit(BTUSB_BULK_RUNNING, &data->flags))
		return;

	usb_anchor_urb(urb, &data->bulk_anchor);
	usb_mark_last_busy(data->udev);

	err = usb_submit_urb(urb, GFP_ATOMIC);
	if (err < 0) {
		/* -EPERM: urb is being killed;
		 * -ENODEV: device got disconnected */
		if (err != -EPERM && err != -ENODEV)
			BT_ERR("%s urb %p failed to resubmit (%d)",
			       hdev->name, urb, -err);
		usb_unanchor_urb(urb);
	}
}

static int btusb_submit_bulk_urb(struct hci_dev *hdev, gfp_t mem_flags)
{
	struct btusb_data *data = hci_get_drvdata(hdev);
	struct urb *urb;
	unsigned char *buf;
	unsigned int pipe;
	int err, size = HCI_MAX_FRAME_SIZE;

	BT_DBG("%s", hdev->name);

	if (!data->bulk_rx_ep)
		return -ENODEV;

	urb = usb_alloc_urb(0, mem_flags);
	if (!urb)
		return -ENOMEM;

	buf = kmalloc(size, mem_flags);
	if (!buf) {
		usb_free_urb(urb);
		return -ENOMEM;
	}

	pipe = usb_rcvbulkpipe(data->udev, data->bulk_rx_ep->bEndpointAddress);

	usb_fill_bulk_urb(urb, data->udev, pipe, buf, size,
			  btusb_bulk_complete, hdev);

	urb->transfer_flags |= URB_FREE_BUFFER;

	usb_mark_last_busy(data->udev);
	usb_anchor_urb(urb, &data->bulk_anchor);

	err = usb_submit_urb(urb, mem_flags);
	if (err < 0) {
		if (err != -EPERM && err != -ENODEV)
			BT_ERR("%s urb %p submission failed (%d)",
			       hdev->name, urb, -err);
		usb_unanchor_urb(urb);
	}

	usb_free_urb(urb);

	return err;
}

static void btusb_isoc_complete(struct urb *urb)
{
	struct hci_dev *hdev = urb->context;
	struct btusb_data *data = hci_get_drvdata(hdev);
	int i, err;

	BT_DBG("%s urb %p status %d count %d", hdev->name, urb, urb->status,
	       urb->actual_length);

	if (!test_bit(HCI_RUNNING, &hdev->flags))
		return;

	if (urb->status == 0) {
		for (i = 0; i < urb->number_of_packets; i++) {
			unsigned int offset = urb->iso_frame_desc[i].offset;
			unsigned int length = urb->iso_frame_desc[i].actual_length;

			if (urb->iso_frame_desc[i].status)
				continue;

			hdev->stat.byte_rx += length;

			if (btusb_recv_isoc(data, urb->transfer_buffer + offset,
					    length) < 0) {
				BT_ERR("%s corrupted SCO packet", hdev->name);
				hdev->stat.err_rx++;
			}
		}
	} else if (urb->status == -ENOENT) {
		/* Avoid suspend failed when usb_kill_urb */
		return;
	}

	if (!test_bit(BTUSB_ISOC_RUNNING, &data->flags))
		return;

	usb_anchor_urb(urb, &data->isoc_anchor);

	err = usb_submit_urb(urb, GFP_ATOMIC);
	if (err < 0) {
		/* -EPERM: urb is being killed;
		 * -ENODEV: device got disconnected */
		if (err != -EPERM && err != -ENODEV)
			BT_ERR("%s urb %p failed to resubmit (%d)",
			       hdev->name, urb, -err);
		usb_unanchor_urb(urb);
	}
}

static inline void __fill_isoc_descriptor(struct urb *urb, int len, int mtu)
{
	int i, offset = 0;

	BT_DBG("len %d mtu %d", len, mtu);

	for (i = 0; i < BTUSB_MAX_ISOC_FRAMES && len >= mtu;
					i++, offset += mtu, len -= mtu) {
		urb->iso_frame_desc[i].offset = offset;
		urb->iso_frame_desc[i].length = mtu;
	}

	if (len && i < BTUSB_MAX_ISOC_FRAMES) {
		urb->iso_frame_desc[i].offset = offset;
		urb->iso_frame_desc[i].length = len;
		i++;
	}

	urb->number_of_packets = i;
}

static int btusb_submit_isoc_urb(struct hci_dev *hdev, gfp_t mem_flags)
{
	struct btusb_data *data = hci_get_drvdata(hdev);
	struct urb *urb;
	unsigned char *buf;
	unsigned int pipe;
	int err, size;

	BT_DBG("%s", hdev->name);

	if (!data->isoc_rx_ep)
		return -ENODEV;

	urb = usb_alloc_urb(BTUSB_MAX_ISOC_FRAMES, mem_flags);
	if (!urb)
		return -ENOMEM;

	size = le16_to_cpu(data->isoc_rx_ep->wMaxPacketSize) *
						BTUSB_MAX_ISOC_FRAMES;

	buf = kmalloc(size, mem_flags);
	if (!buf) {
		usb_free_urb(urb);
		return -ENOMEM;
	}

	pipe = usb_rcvisocpipe(data->udev, data->isoc_rx_ep->bEndpointAddress);

	usb_fill_int_urb(urb, data->udev, pipe, buf, size, btusb_isoc_complete,
			 hdev, data->isoc_rx_ep->bInterval);

	urb->transfer_flags = URB_FREE_BUFFER | URB_ISO_ASAP;

	__fill_isoc_descriptor(urb, size,
			       le16_to_cpu(data->isoc_rx_ep->wMaxPacketSize));

	usb_anchor_urb(urb, &data->isoc_anchor);

	err = usb_submit_urb(urb, mem_flags);
	if (err < 0) {
		if (err != -EPERM && err != -ENODEV)
			BT_ERR("%s urb %p submission failed (%d)",
			       hdev->name, urb, -err);
		usb_unanchor_urb(urb);
	}

	usb_free_urb(urb);

	return err;
}

static void btusb_tx_complete(struct urb *urb)
{
	struct sk_buff *skb = urb->context;
	struct hci_dev *hdev = (struct hci_dev *)skb->dev;
	struct btusb_data *data = hci_get_drvdata(hdev);

	BT_DBG("%s urb %p status %d count %d", hdev->name, urb, urb->status,
	       urb->actual_length);

	if (!test_bit(HCI_RUNNING, &hdev->flags))
		goto done;

	if (!urb->status)
		hdev->stat.byte_tx += urb->transfer_buffer_length;
	else
		hdev->stat.err_tx++;

done:
	spin_lock(&data->txlock);
	data->tx_in_flight--;
	spin_unlock(&data->txlock);

	kfree(urb->setup_packet);

	kfree_skb(skb);
}

static void btusb_isoc_tx_complete(struct urb *urb)
{
	struct sk_buff *skb = urb->context;
	struct hci_dev *hdev = (struct hci_dev *)skb->dev;

	BT_DBG("%s urb %p status %d count %d", hdev->name, urb, urb->status,
	       urb->actual_length);

	if (!test_bit(HCI_RUNNING, &hdev->flags))
		goto done;

	if (!urb->status)
		hdev->stat.byte_tx += urb->transfer_buffer_length;
	else
		hdev->stat.err_tx++;

done:
	kfree(urb->setup_packet);

	kfree_skb(skb);
}

static int btusb_open(struct hci_dev *hdev)
{
	struct btusb_data *data = hci_get_drvdata(hdev);
	int err;

	BT_DBG("%s", hdev->name);

	err = usb_autopm_get_interface(data->intf);
	if (err < 0)
		return err;

	data->intf->needs_remote_wakeup = 1;

	if (test_and_set_bit(HCI_RUNNING, &hdev->flags))
		goto done;

	if (test_and_set_bit(BTUSB_INTR_RUNNING, &data->flags))
		goto done;

	err = btusb_submit_intr_urb(hdev, GFP_KERNEL);
	if (err < 0)
		goto failed;

	err = btusb_submit_bulk_urb(hdev, GFP_KERNEL);
	if (err < 0) {
		usb_kill_anchored_urbs(&data->intr_anchor);
		goto failed;
	}

	set_bit(BTUSB_BULK_RUNNING, &data->flags);
	btusb_submit_bulk_urb(hdev, GFP_KERNEL);

done:
	usb_autopm_put_interface(data->intf);
	return 0;

failed:
	clear_bit(BTUSB_INTR_RUNNING, &data->flags);
	clear_bit(HCI_RUNNING, &hdev->flags);
	usb_autopm_put_interface(data->intf);
	return err;
}

static void btusb_stop_traffic(struct btusb_data *data)
{
	usb_kill_anchored_urbs(&data->intr_anchor);
	usb_kill_anchored_urbs(&data->bulk_anchor);
	usb_kill_anchored_urbs(&data->isoc_anchor);
}

static int btusb_close(struct hci_dev *hdev)
{
	struct btusb_data *data = hci_get_drvdata(hdev);
	int err;

	BT_DBG("%s", hdev->name);

	if (!test_and_clear_bit(HCI_RUNNING, &hdev->flags))
		return 0;

	cancel_work_sync(&data->work);
	cancel_work_sync(&data->waker);

	clear_bit(BTUSB_ISOC_RUNNING, &data->flags);
	clear_bit(BTUSB_BULK_RUNNING, &data->flags);
	clear_bit(BTUSB_INTR_RUNNING, &data->flags);

	btusb_stop_traffic(data);
	btusb_free_frags(data);

	err = usb_autopm_get_interface(data->intf);
	if (err < 0)
		goto failed;

	data->intf->needs_remote_wakeup = 0;
	usb_autopm_put_interface(data->intf);

failed:
	usb_scuttle_anchored_urbs(&data->deferred);
	return 0;
}

static int btusb_flush(struct hci_dev *hdev)
{
	struct btusb_data *data = hci_get_drvdata(hdev);

	BT_DBG("%s", hdev->name);

	usb_kill_anchored_urbs(&data->tx_anchor);
	btusb_free_frags(data);

	return 0;
}

static struct urb *alloc_ctrl_urb(struct hci_dev *hdev, struct sk_buff *skb)
{
	struct btusb_data *data = hci_get_drvdata(hdev);
	struct usb_ctrlrequest *dr;
	struct urb *urb;
	unsigned int pipe;

	urb = usb_alloc_urb(0, GFP_KERNEL);
	if (!urb)
		return ERR_PTR(-ENOMEM);

	dr = kmalloc(sizeof(*dr), GFP_KERNEL);
	if (!dr) {
		usb_free_urb(urb);
		return ERR_PTR(-ENOMEM);
	}

	dr->bRequestType = data->cmdreq_type;
	dr->bRequest     = data->cmdreq;
	dr->wIndex       = 0;
	dr->wValue       = 0;
	dr->wLength      = __cpu_to_le16(skb->len);

	pipe = usb_sndctrlpipe(data->udev, 0x00);

	usb_fill_control_urb(urb, data->udev, pipe, (void *)dr,
			     skb->data, skb->len, btusb_tx_complete, skb);

	skb->dev = (void *)hdev;

	return urb;
}

static struct urb *alloc_bulk_urb(struct hci_dev *hdev, struct sk_buff *skb)
{
	struct btusb_data *data = hci_get_drvdata(hdev);
	struct urb *urb;
	unsigned int pipe;

	if (!data->bulk_tx_ep)
		return ERR_PTR(-ENODEV);

	urb = usb_alloc_urb(0, GFP_KERNEL);
	if (!urb)
		return ERR_PTR(-ENOMEM);

	pipe = usb_sndbulkpipe(data->udev, data->bulk_tx_ep->bEndpointAddress);

	usb_fill_bulk_urb(urb, data->udev, pipe,
			  skb->data, skb->len, btusb_tx_complete, skb);

	skb->dev = (void *)hdev;

	return urb;
}

static struct urb *alloc_isoc_urb(struct hci_dev *hdev, struct sk_buff *skb)
{
	struct btusb_data *data = hci_get_drvdata(hdev);
	struct urb *urb;
	unsigned int pipe;

	if (!data->isoc_tx_ep)
		return ERR_PTR(-ENODEV);

	urb = usb_alloc_urb(BTUSB_MAX_ISOC_FRAMES, GFP_KERNEL);
	if (!urb)
		return ERR_PTR(-ENOMEM);

	pipe = usb_sndisocpipe(data->udev, data->isoc_tx_ep->bEndpointAddress);

	usb_fill_int_urb(urb, data->udev, pipe,
			 skb->data, skb->len, btusb_isoc_tx_complete,
			 skb, data->isoc_tx_ep->bInterval);

	urb->transfer_flags  = URB_ISO_ASAP;

	__fill_isoc_descriptor(urb, skb->len,
			       le16_to_cpu(data->isoc_tx_ep->wMaxPacketSize));

	skb->dev = (void *)hdev;

	return urb;
}

static int submit_tx_urb(struct hci_dev *hdev, struct urb *urb)
{
	struct btusb_data *data = hci_get_drvdata(hdev);
	int err;

	usb_anchor_urb(urb, &data->tx_anchor);

	err = usb_submit_urb(urb, GFP_KERNEL);
	if (err < 0) {
		if (err != -EPERM && err != -ENODEV)
			BT_ERR("%s urb %p submission failed (%d)",
			       hdev->name, urb, -err);
		kfree(urb->setup_packet);
		usb_unanchor_urb(urb);
	} else {
		usb_mark_last_busy(data->udev);
	}

	usb_free_urb(urb);
	return err;
}

static int submit_or_queue_tx_urb(struct hci_dev *hdev, struct urb *urb)
{
	struct btusb_data *data = hci_get_drvdata(hdev);
	unsigned long flags;
	bool suspending;

	spin_lock_irqsave(&data->txlock, flags);
	suspending = test_bit(BTUSB_SUSPENDING, &data->flags);
	if (!suspending)
		data->tx_in_flight++;
	spin_unlock_irqrestore(&data->txlock, flags);

	if (!suspending)
		return submit_tx_urb(hdev, urb);

	usb_anchor_urb(urb, &data->deferred);
	schedule_work(&data->waker);

	usb_free_urb(urb);
	return 0;
}

static int btusb_send_frame(struct hci_dev *hdev, struct sk_buff *skb)
{
	struct urb *urb;

	BT_DBG("%s", hdev->name);

	if (!test_bit(HCI_RUNNING, &hdev->flags))
		return -EBUSY;

	switch (bt_cb(skb)->pkt_type) {
	case HCI_COMMAND_PKT:
		urb = alloc_ctrl_urb(hdev, skb);
		if (IS_ERR(urb))
			return PTR_ERR(urb);

		hdev->stat.cmd_tx++;
		return submit_or_queue_tx_urb(hdev, urb);

	case HCI_ACLDATA_PKT:
		urb = alloc_bulk_urb(hdev, skb);
		if (IS_ERR(urb))
			return PTR_ERR(urb);

		hdev->stat.acl_tx++;
		return submit_or_queue_tx_urb(hdev, urb);

	case HCI_SCODATA_PKT:
		if (hci_conn_num(hdev, SCO_LINK) < 1)
			return -ENODEV;

		urb = alloc_isoc_urb(hdev, skb);
		if (IS_ERR(urb))
			return PTR_ERR(urb);

		hdev->stat.sco_tx++;
		return submit_tx_urb(hdev, urb);
	}

	return -EILSEQ;
}

static void btusb_notify(struct hci_dev *hdev, unsigned int evt)
{
	struct btusb_data *data = hci_get_drvdata(hdev);

	BT_DBG("%s evt %d", hdev->name, evt);

	if (hci_conn_num(hdev, SCO_LINK) != data->sco_num) {
		data->sco_num = hci_conn_num(hdev, SCO_LINK);
		schedule_work(&data->work);
	}
}

static inline int __set_isoc_interface(struct hci_dev *hdev, int altsetting)
{
	struct btusb_data *data = hci_get_drvdata(hdev);
	struct usb_interface *intf = data->isoc;
	struct usb_endpoint_descriptor *ep_desc;
	int i, err;

	if (!data->isoc)
		return -ENODEV;

	err = usb_set_interface(data->udev, 1, altsetting);
	if (err < 0) {
		BT_ERR("%s setting interface failed (%d)", hdev->name, -err);
		return err;
	}

	data->isoc_altsetting = altsetting;

	data->isoc_tx_ep = NULL;
	data->isoc_rx_ep = NULL;

	for (i = 0; i < intf->cur_altsetting->desc.bNumEndpoints; i++) {
		ep_desc = &intf->cur_altsetting->endpoint[i].desc;

		if (!data->isoc_tx_ep && usb_endpoint_is_isoc_out(ep_desc)) {
			data->isoc_tx_ep = ep_desc;
			continue;
		}

		if (!data->isoc_rx_ep && usb_endpoint_is_isoc_in(ep_desc)) {
			data->isoc_rx_ep = ep_desc;
			continue;
		}
	}

	if (!data->isoc_tx_ep || !data->isoc_rx_ep) {
		BT_ERR("%s invalid SCO descriptors", hdev->name);
		return -ENODEV;
	}

	return 0;
}

static void btusb_work(struct work_struct *work)
{
	struct btusb_data *data = container_of(work, struct btusb_data, work);
	struct hci_dev *hdev = data->hdev;
	int new_alts;
	int err;

	if (data->sco_num > 0) {
		if (!test_bit(BTUSB_DID_ISO_RESUME, &data->flags)) {
			err = usb_autopm_get_interface(data->isoc ? data->isoc : data->intf);
			if (err < 0) {
				clear_bit(BTUSB_ISOC_RUNNING, &data->flags);
				usb_kill_anchored_urbs(&data->isoc_anchor);
				return;
			}

			set_bit(BTUSB_DID_ISO_RESUME, &data->flags);
		}

		if (hdev->voice_setting & 0x0020) {
			static const int alts[3] = { 2, 4, 5 };

			new_alts = alts[data->sco_num - 1];
		} else {
			new_alts = data->sco_num;
		}

		if (data->isoc_altsetting != new_alts) {
			clear_bit(BTUSB_ISOC_RUNNING, &data->flags);
			usb_kill_anchored_urbs(&data->isoc_anchor);

			if (__set_isoc_interface(hdev, new_alts) < 0)
				return;
		}

		if (!test_and_set_bit(BTUSB_ISOC_RUNNING, &data->flags)) {
			if (btusb_submit_isoc_urb(hdev, GFP_KERNEL) < 0)
				clear_bit(BTUSB_ISOC_RUNNING, &data->flags);
			else
				btusb_submit_isoc_urb(hdev, GFP_KERNEL);
		}
	} else {
		clear_bit(BTUSB_ISOC_RUNNING, &data->flags);
		usb_kill_anchored_urbs(&data->isoc_anchor);

		__set_isoc_interface(hdev, 0);
		if (test_and_clear_bit(BTUSB_DID_ISO_RESUME, &data->flags))
			usb_autopm_put_interface(data->isoc ? data->isoc : data->intf);
	}
}

static void btusb_waker(struct work_struct *work)
{
	struct btusb_data *data = container_of(work, struct btusb_data, waker);
	int err;

	err = usb_autopm_get_interface(data->intf);
	if (err < 0)
		return;

	usb_autopm_put_interface(data->intf);
}

static struct sk_buff *btusb_read_local_version(struct hci_dev *hdev)
{
	struct hci_rp_read_local_version *ver;
	struct sk_buff *skb = __hci_cmd_sync(hdev, HCI_OP_READ_LOCAL_VERSION,
					     0, NULL, HCI_INIT_TIMEOUT);

	if (IS_ERR(skb)) {
		BT_ERR("%s: HCI_OP_READ_LOCAL_VERSION failed (%ld)",
		       hdev->name, PTR_ERR(skb));
		return skb;
	}

	if (skb->len != sizeof(*ver)) {
		BT_ERR("%s: HCI_OP_READ_LOCAL_VERSION event length mismatch",
		       hdev->name);
		kfree_skb(skb);
		return ERR_PTR(-EIO);
	}

	ver = (struct hci_rp_read_local_version *) skb->data;
	BT_INFO("%s: hci_ver=%02x hci_rev=%04x lmp_ver=%02x lmp_subver=%04x",
		hdev->name, ver->hci_ver, ver->hci_rev, ver->lmp_ver,
		ver->lmp_subver);

	return skb;
}

static int btusb_setup_bcm92035(struct hci_dev *hdev)
{
	struct sk_buff *skb;
	u8 val = 0x00;

	BT_DBG("%s", hdev->name);

	skb = __hci_cmd_sync(hdev, 0xfc3b, 1, &val, HCI_INIT_TIMEOUT);
	if (IS_ERR(skb))
		BT_ERR("BCM92035 command failed (%ld)", -PTR_ERR(skb));
	else
		kfree_skb(skb);

	return 0;
}

static int btusb_setup_csr(struct hci_dev *hdev)
{
	struct hci_rp_read_local_version *rp;
	struct sk_buff *skb;
	int ret;

	BT_DBG("%s", hdev->name);

	skb = btusb_read_local_version(hdev);
	if (IS_ERR(skb))
		return -PTR_ERR(skb);

	rp = (struct hci_rp_read_local_version *)skb->data;

	if (!rp->status) {
		if (le16_to_cpu(rp->manufacturer) != 10) {
			/* Clear the reset quirk since this is not an actual
			 * early Bluetooth 1.1 device from CSR.
			 */
			clear_bit(HCI_QUIRK_RESET_ON_CLOSE, &hdev->quirks);

			/* These fake CSR controllers have all a broken
			 * stored link key handling and so just disable it.
			 */
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(3, 14, 0))
			set_bit(HCI_QUIRK_BROKEN_STORED_LINK_KEY,
				&hdev->quirks);
#endif
		}
	}

	ret = -bt_to_errno(rp->status);

	kfree_skb(skb);

	return ret;
}

#define RTL_FRAG_LEN 252

struct rtl_download_cmd {
	uint8_t index;
	uint8_t data[RTL_FRAG_LEN];
} __packed;

struct rtl_download_response {
	uint8_t status;
	uint8_t index;
} __packed;

struct rtl_rom_version_evt {
	uint8_t status;
	uint8_t version;
} __packed;

struct rtl_epatch_header {
	uint8_t signature[8];
	uint32_t fw_version;
	uint16_t num_patches;
} __packed;

static const uint8_t RTL_EPATCH_SIGNATURE[] = {
	0x52, 0x65, 0x61, 0x6C, 0x74, 0x65, 0x63, 0x68
};

#define RTL_ROM_LMP_3499	0x3499
#define RTL_ROM_LMP_8723A	0x1200
#define RTL_ROM_LMP_8723B	0x8723
#define RTL_ROM_LMP_8723B2	0x4ce1
#define RTL_ROM_LMP_8821A	0x8821
#define RTL_ROM_LMP_8761A	0x8761

static int rtl_read_rom_version(struct hci_dev *hdev)
{
	struct rtl_rom_version_evt *rom_version;
	struct sk_buff *skb;
	int r;

	/* Read RTL ROM version command */
	skb = __hci_cmd_sync(hdev, 0xfc6d, 0, NULL, HCI_INIT_TIMEOUT);
	if (IS_ERR(skb)) {
		BT_ERR("Read ROM version failed (%ld)", PTR_ERR(skb));
		return PTR_ERR(skb);
	}

	if (skb->len != sizeof(*rom_version)) {
		BT_ERR("RTL version event length mismatch");
		kfree_skb(skb);
		return -EIO;
	}

	rom_version = (struct rtl_rom_version_evt *) skb->data;
	BT_DBG("rom_version status=%x version=%x",
	       rom_version->status, rom_version->version);
	if (rom_version->status)
		r = 0;
	else
		r = rom_version->version;

	kfree_skb(skb);
	return r;
}

static int rtl8723b_parse_firmware(struct hci_dev *hdev, u16 lmp_subver,
				   const struct firmware *fw,
				   unsigned char **_buf)
{
	const uint8_t extension_sig[] = { 0x51, 0x04, 0xfd, 0x77 };
	struct rtl_epatch_header *epatch_info;
	unsigned char *buf;
	int i, len, rom_version;
	size_t min_size;
	uint8_t opcode, length, data;
	int project_id = -1;
	const unsigned char *fwptr, *chip_id_base;
	const unsigned char *patch_length_base, *patch_offset_base;
	u32 patch_offset = 0;
	u16 patch_length;
	const uint16_t project_id_to_lmp_subver[] = {
		RTL_ROM_LMP_8723A,
		RTL_ROM_LMP_8723B,
		RTL_ROM_LMP_8821A,
		RTL_ROM_LMP_8761A,
		RTL_ROM_LMP_8723B2,
	};

	rom_version = rtl_read_rom_version(hdev);
	if (rom_version < 0)
		return rom_version;

	BT_DBG("lmp_subver=%x rom_version=%x", lmp_subver, rom_version);

	min_size = sizeof(struct rtl_epatch_header) + sizeof(extension_sig) + 3;
	if (fw->size < min_size)
		return -EINVAL;

	fwptr = fw->data + fw->size - sizeof(extension_sig);
	if (memcmp(fwptr, extension_sig, sizeof(extension_sig)) != 0) {
		BT_ERR("extension section signature mismatch");
		return -EINVAL;
	}

	/* Loop from the end of the firmware parsing instructions, until
	 * we find an instruction that identifies the "project ID" for the
	 * hardware supported by this firwmare file.
	 * Once we have that, we double-check that that project_id is suitable
	 * for the hardware we are working with. */
	while (fwptr >= fw->data + (sizeof(struct rtl_epatch_header) + 3)) {
		opcode = *--fwptr;
		length = *--fwptr;
		data = *--fwptr;
		pr_info("check op=%x len=%x data=%x", opcode, length, data);

		if (opcode == 0xff) /* EOF */
			break;

		if (length == 0) {
			BT_ERR("found instruction with length 0");
			return -EINVAL;
		}

		if (opcode == 0 && length == 1) {
			project_id = data;
			break;
		}

		fwptr -= length;
	}

	if (project_id < 0) {
		BT_ERR("failed to find version instruction");
		return -EINVAL;
	}

	if (project_id > ARRAY_SIZE(project_id_to_lmp_subver)) {
		BT_ERR("unknown project id %d", project_id);
		return -EINVAL;
	}

	if (lmp_subver != project_id_to_lmp_subver[project_id]) {
		BT_ERR("firmware is for %x but this is a %x, project_id is %d",
		       project_id_to_lmp_subver[project_id], lmp_subver, project_id);
		return -EINVAL;
	}

	epatch_info = (struct rtl_epatch_header *) fw->data;
	if (memcmp(epatch_info->signature, RTL_EPATCH_SIGNATURE, 8) != 0) {
		BT_ERR("bad EPATCH signature");
		return -EINVAL;
	}

	BT_DBG("fw_version=%x, num_patches=%d",
	       epatch_info->fw_version, epatch_info->num_patches);

	/* After the rtl_epatch_header there is a funky patch metadata section.
	 * Assuming 2 patches, the layout is:
	 * ChipID1 ChipID2 PatchLength1 PatchLength2 PatchOffset1 PatchOffset2
	 *
	 * Find the right patch for this chip. */
	min_size += 8 * epatch_info->num_patches;
	if (fw->size < min_size)
		return -EINVAL;

	chip_id_base = fw->data + sizeof(struct rtl_epatch_header);
	patch_length_base = chip_id_base +
			    (sizeof(u16) * epatch_info->num_patches);
	patch_offset_base = patch_length_base +
			    (sizeof(u16) * epatch_info->num_patches);
	for (i = 0; i < epatch_info->num_patches; i++) {
		u16 chip_id = get_unaligned_le16(chip_id_base +
						 (i * sizeof(u16)));
		if (chip_id == rom_version + 1) {
			patch_length = get_unaligned_le16(patch_length_base +
							  (i * sizeof(u16)));
			patch_offset = get_unaligned_le32(patch_offset_base +
							  (i * sizeof(u32)));
			break;
		}
	}

	if (!patch_offset) {
		BT_ERR("didn't find patch for chip id %d", rom_version);
		return -EINVAL;
	}

	BT_DBG("length=%x offset=%x index %d", patch_length, patch_offset, i);
	min_size = patch_offset + patch_length;
	if (fw->size < min_size)
		return -EINVAL;

	/* Copy the firmware into a new buffer and write the version at
	 * the end. */
	len = patch_length;
	buf = kmemdup(fw->data + patch_offset, patch_length, GFP_KERNEL);
	if (!buf)
		return -ENOMEM;

	memcpy(buf + patch_length - 4, &epatch_info->fw_version, 4);

	*_buf = buf;
	return len;
}

static int rtl_download_firmware(struct hci_dev *hdev,
				 const unsigned char *data, int fw_len)
{
	struct rtl_download_cmd *dl_cmd;
	int frag_num = fw_len / RTL_FRAG_LEN + 1;
	int frag_len = RTL_FRAG_LEN;
	int ret = 0;
	int i;

	dl_cmd = kmalloc(sizeof(struct rtl_download_cmd), GFP_KERNEL);
	if (!dl_cmd)
		return -ENOMEM;

	for (i = 0; i < frag_num; i++) {
		struct rtl_download_response *dl_resp;
		struct sk_buff *skb;

		BT_DBG("download fw (%d/%d)", i, frag_num);
		dl_cmd->index = i;
		if (i == (frag_num - 1)) {
			dl_cmd->index |= 0x80; /* data end */
			frag_len = fw_len % RTL_FRAG_LEN;
		}
		memcpy(dl_cmd->data, data, frag_len);

		/* Send download command */
		skb = __hci_cmd_sync(hdev, 0xfc20, frag_len + 1, dl_cmd,
				     HCI_INIT_TIMEOUT);
		if (IS_ERR(skb)) {
			BT_ERR("download fw command failed (%ld)",
			       PTR_ERR(skb));
			ret = -PTR_ERR(skb);
			goto out;
		}

		if (skb->len != sizeof(*dl_resp)) {
			BT_ERR("download fw event length mismatch");
			kfree_skb(skb);
			ret = -EIO;
			goto out;
		}

		dl_resp = (struct rtl_download_response *) skb->data;
		if (dl_resp->status != 0) {
			kfree_skb(skb);
			ret = bt_to_errno(dl_resp->status);
			goto out;
		}

		kfree_skb(skb);
		data += RTL_FRAG_LEN;
	}

out:
	kfree(dl_cmd);
	return ret;
}

static int btusb_setup_rtl8723a(struct hci_dev *hdev)
{
	struct btusb_data *data = dev_get_drvdata(&hdev->dev);
	struct usb_device *udev = interface_to_usbdev(data->intf);
	struct sk_buff *skb;
	struct hci_rp_read_local_version *resp;
	const struct firmware *fw;
	int ret;

	skb = btusb_read_local_version(hdev);
	if (IS_ERR(skb))
		return -PTR_ERR(skb);

	resp = (struct hci_rp_read_local_version *) skb->data;
	if (resp->lmp_subver != RTL_ROM_LMP_8723A
			&& resp->lmp_subver != RTL_ROM_LMP_3499) {
		BT_INFO("rtl8723a: assuming no firmware upload needed.");
		kfree_skb(skb);
		return 0;
	}

	kfree_skb(skb);

	ret = request_firmware(&fw, "rtl_bt/rtl8723a_fw.bin", &udev->dev);
	if (ret < 0) {
		BT_ERR("Failed to load rtl_bt/rtl8723a_fw.bin");
		return ret;
	}

	if (fw->size < 8) {
		ret = -EINVAL;
		goto out;
	}

	/* Check that the firmware doesn't have the epatch signature
	 * (which is only for RTL8723B and newer). */
	if (memcmp(fw->data, RTL_EPATCH_SIGNATURE, 8) == 0) {
		BT_ERR("RTL8723A: unexpected EPATCH signature!");
		ret = -EINVAL;
		goto out;
	}

	ret = rtl_download_firmware(hdev, fw->data, fw->size);

out:
	release_firmware(fw);
	return ret;
}

static int btusb_setup_rtl8723b(struct hci_dev *hdev)
{
	struct btusb_data *data = dev_get_drvdata(&hdev->dev);
	struct usb_device *udev = interface_to_usbdev(data->intf);
	const char *fw_name;
	unsigned char *fw_data;
	u16 lmp_subver;
	const struct firmware *fw;
	struct hci_rp_read_local_version *resp;
	struct sk_buff *skb;
	int ret;

	skb = btusb_read_local_version(hdev);
	if (IS_ERR(skb))
		return -PTR_ERR(skb);

	resp = (struct hci_rp_read_local_version *) skb->data;
	if (resp->status) {
		BT_ERR("rtl fw version event failed (%02x)", resp->status);
		kfree_skb(skb);
		return bt_to_errno(resp->status);
	}

	lmp_subver = resp->lmp_subver;
	kfree_skb(skb);

	switch (lmp_subver) {
	case RTL_ROM_LMP_8723B:
	case RTL_ROM_LMP_8723B2:
		fw_name = "rtl_bt/rtl8723b_fw.bin";
		break;
	case RTL_ROM_LMP_8821A:
		fw_name = "rtl_bt/rtl8821a_fw.bin";
		break;
	case RTL_ROM_LMP_8761A:
		fw_name = "rtl_bt/rtl8761a_fw.bin";
		break;
	default:
		BT_INFO("rtl8723b: assuming no firmware upload needed.");
		return 0;
	}

	ret = request_firmware(&fw, fw_name, &udev->dev);
	if (ret < 0) {
		BT_ERR("Failed to load %s", fw_name);
		return ret;
	}

	pr_info("btusb: Loaded firmware %s\n", fw_name);
	ret = rtl8723b_parse_firmware(hdev, lmp_subver, fw, &fw_data);
	if (ret < 0)
		goto out;

	ret = rtl_download_firmware(hdev, fw_data, ret);
	kfree(fw_data);
	if (ret < 0)
		goto out;

out:
	release_firmware(fw);
	return ret;
}

struct intel_version {
	u8 status;
	u8 hw_platform;
	u8 hw_variant;
	u8 hw_revision;
	u8 fw_variant;
	u8 fw_revision;
	u8 fw_build_num;
	u8 fw_build_ww;
	u8 fw_build_yy;
	u8 fw_patch_num;
} __packed;

struct intel_boot_params {
	__u8     status;
	__u8     otp_format;
	__u8     otp_content;
	__u8     otp_patch;
	__le16   dev_revid;
	__u8     secure_boot;
	__u8     key_from_hdr;
	__u8     key_type;
	__u8     otp_lock;
	__u8     api_lock;
	__u8     debug_lock;
	bdaddr_t otp_bdaddr;
	__u8     min_fw_build_nn;
	__u8     min_fw_build_cw;
	__u8     min_fw_build_yy;
	__u8     limited_cce;
	__u8     unlocked_state;
} __packed;

static const struct firmware *btusb_setup_intel_get_fw(struct hci_dev *hdev,
						       struct intel_version *ver)
{
	const struct firmware *fw;
	char fwname[64];
	int ret;

	snprintf(fwname, sizeof(fwname),
		 "intel/ibt-hw-%x.%x.%x-fw-%x.%x.%x.%x.%x.bseq",
		 ver->hw_platform, ver->hw_variant, ver->hw_revision,
		 ver->fw_variant,  ver->fw_revision, ver->fw_build_num,
		 ver->fw_build_ww, ver->fw_build_yy);

	ret = request_firmware(&fw, fwname, &hdev->dev);
	if (ret < 0) {
		if (ret == -EINVAL) {
			BT_ERR("%s Intel firmware file request failed (%d)",
			       hdev->name, ret);
			return NULL;
		}

		BT_ERR("%s failed to open Intel firmware file: %s(%d)",
		       hdev->name, fwname, ret);

		/* If the correct firmware patch file is not found, use the
		 * default firmware patch file instead
		 */
		snprintf(fwname, sizeof(fwname), "intel/ibt-hw-%x.%x.bseq",
			 ver->hw_platform, ver->hw_variant);
		if (request_firmware(&fw, fwname, &hdev->dev) < 0) {
			BT_ERR("%s failed to open default Intel fw file: %s",
			       hdev->name, fwname);
			return NULL;
		}
	}

	BT_INFO("%s: Intel Bluetooth firmware file: %s", hdev->name, fwname);

	return fw;
}

static int btusb_setup_intel_patching(struct hci_dev *hdev,
				      const struct firmware *fw,
				      const u8 **fw_ptr, int *disable_patch)
{
	struct sk_buff *skb;
	struct hci_command_hdr *cmd;
	const u8 *cmd_param;
	struct hci_event_hdr *evt = NULL;
	const u8 *evt_param = NULL;
	int remain = fw->size - (*fw_ptr - fw->data);

	/* The first byte indicates the types of the patch command or event.
	 * 0x01 means HCI command and 0x02 is HCI event. If the first bytes
	 * in the current firmware buffer doesn't start with 0x01 or
	 * the size of remain buffer is smaller than HCI command header,
	 * the firmware file is corrupted and it should stop the patching
	 * process.
	 */
	if (remain > HCI_COMMAND_HDR_SIZE && *fw_ptr[0] != 0x01) {
		BT_ERR("%s Intel fw corrupted: invalid cmd read", hdev->name);
		return -EINVAL;
	}
	(*fw_ptr)++;
	remain--;

	cmd = (struct hci_command_hdr *)(*fw_ptr);
	*fw_ptr += sizeof(*cmd);
	remain -= sizeof(*cmd);

	/* Ensure that the remain firmware data is long enough than the length
	 * of command parameter. If not, the firmware file is corrupted.
	 */
	if (remain < cmd->plen) {
		BT_ERR("%s Intel fw corrupted: invalid cmd len", hdev->name);
		return -EFAULT;
	}

	/* If there is a command that loads a patch in the firmware
	 * file, then enable the patch upon success, otherwise just
	 * disable the manufacturer mode, for example patch activation
	 * is not required when the default firmware patch file is used
	 * because there are no patch data to load.
	 */
	if (*disable_patch && le16_to_cpu(cmd->opcode) == 0xfc8e)
		*disable_patch = 0;

	cmd_param = *fw_ptr;
	*fw_ptr += cmd->plen;
	remain -= cmd->plen;

	/* This reads the expected events when the above command is sent to the
	 * device. Some vendor commands expects more than one events, for
	 * example command status event followed by vendor specific event.
	 * For this case, it only keeps the last expected event. so the command
	 * can be sent with __hci_cmd_sync_ev() which returns the sk_buff of
	 * last expected event.
	 */
	while (remain > HCI_EVENT_HDR_SIZE && *fw_ptr[0] == 0x02) {
		(*fw_ptr)++;
		remain--;

		evt = (struct hci_event_hdr *)(*fw_ptr);
		*fw_ptr += sizeof(*evt);
		remain -= sizeof(*evt);

		if (remain < evt->plen) {
			BT_ERR("%s Intel fw corrupted: invalid evt len",
			       hdev->name);
			return -EFAULT;
		}

		evt_param = *fw_ptr;
		*fw_ptr += evt->plen;
		remain -= evt->plen;
	}

	/* Every HCI commands in the firmware file has its correspond event.
	 * If event is not found or remain is smaller than zero, the firmware
	 * file is corrupted.
	 */
	if (!evt || !evt_param || remain < 0) {
		BT_ERR("%s Intel fw corrupted: invalid evt read", hdev->name);
		return -EFAULT;
	}

	skb = __hci_cmd_sync_ev(hdev, le16_to_cpu(cmd->opcode), cmd->plen,
				cmd_param, evt->evt, HCI_INIT_TIMEOUT);
	if (IS_ERR(skb)) {
		BT_ERR("%s sending Intel patch command (0x%4.4x) failed (%ld)",
		       hdev->name, cmd->opcode, PTR_ERR(skb));
		return PTR_ERR(skb);
	}

	/* It ensures that the returned event matches the event data read from
	 * the firmware file. At fist, it checks the length and then
	 * the contents of the event.
	 */
	if (skb->len != evt->plen) {
		BT_ERR("%s mismatch event length (opcode 0x%4.4x)", hdev->name,
		       le16_to_cpu(cmd->opcode));
		kfree_skb(skb);
		return -EFAULT;
	}

	if (memcmp(skb->data, evt_param, evt->plen)) {
		BT_ERR("%s mismatch event parameter (opcode 0x%4.4x)",
		       hdev->name, le16_to_cpu(cmd->opcode));
		kfree_skb(skb);
		return -EFAULT;
	}
	kfree_skb(skb);

	return 0;
}

#define BDADDR_INTEL (&(bdaddr_t) {{0x00, 0x8b, 0x9e, 0x19, 0x03, 0x00}})

static int btusb_check_bdaddr_intel(struct hci_dev *hdev)
{
	struct sk_buff *skb;
	struct hci_rp_read_bd_addr *rp;

	skb = __hci_cmd_sync(hdev, HCI_OP_READ_BD_ADDR, 0, NULL,
			     HCI_INIT_TIMEOUT);
	if (IS_ERR(skb)) {
		BT_ERR("%s reading Intel device address failed (%ld)",
		       hdev->name, PTR_ERR(skb));
		return PTR_ERR(skb);
	}

	if (skb->len != sizeof(*rp)) {
		BT_ERR("%s Intel device address length mismatch", hdev->name);
		kfree_skb(skb);
		return -EIO;
	}

	rp = (struct hci_rp_read_bd_addr *)skb->data;
	if (rp->status) {
		BT_ERR("%s Intel device address result failed (%02x)",
		       hdev->name, rp->status);
		kfree_skb(skb);
		return -bt_to_errno(rp->status);
	}

	/* For some Intel based controllers, the default Bluetooth device
	 * address 00:03:19:9E:8B:00 can be found. These controllers are
	 * fully operational, but have the danger of duplicate addresses
	 * and that in turn can cause problems with Bluetooth operation.
	 */
	if (!bacmp(&rp->bdaddr, BDADDR_INTEL)) {
		BT_ERR("%s found Intel default device address (%pMR)",
		       hdev->name, &rp->bdaddr);
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(3, 17, 0))
		set_bit(HCI_QUIRK_INVALID_BDADDR, &hdev->quirks);
#endif
	}

	kfree_skb(skb);

	return 0;
}

static int btusb_setup_intel(struct hci_dev *hdev)
{
	struct sk_buff *skb;
	const struct firmware *fw;
	const u8 *fw_ptr;
	int disable_patch;
	struct intel_version *ver;

	const u8 mfg_enable[] = { 0x01, 0x00 };
	const u8 mfg_disable[] = { 0x00, 0x00 };
	const u8 mfg_reset_deactivate[] = { 0x00, 0x01 };
	const u8 mfg_reset_activate[] = { 0x00, 0x02 };

	BT_DBG("%s", hdev->name);

	/* The controller has a bug with the first HCI command sent to it
	 * returning number of completed commands as zero. This would stall the
	 * command processing in the Bluetooth core.
	 *
	 * As a workaround, send HCI Reset command first which will reset the
	 * number of completed commands and allow normal command processing
	 * from now on.
	 */
	skb = __hci_cmd_sync(hdev, HCI_OP_RESET, 0, NULL, HCI_INIT_TIMEOUT);
	if (IS_ERR(skb)) {
		BT_ERR("%s sending initial HCI reset command failed (%ld)",
		       hdev->name, PTR_ERR(skb));
		return PTR_ERR(skb);
	}
	kfree_skb(skb);

	/* Read Intel specific controller version first to allow selection of
	 * which firmware file to load.
	 *
	 * The returned information are hardware variant and revision plus
	 * firmware variant, revision and build number.
	 */
	skb = __hci_cmd_sync(hdev, 0xfc05, 0, NULL, HCI_INIT_TIMEOUT);
	if (IS_ERR(skb)) {
		BT_ERR("%s reading Intel fw version command failed (%ld)",
		       hdev->name, PTR_ERR(skb));
		return PTR_ERR(skb);
	}

	if (skb->len != sizeof(*ver)) {
		BT_ERR("%s Intel version event length mismatch", hdev->name);
		kfree_skb(skb);
		return -EIO;
	}

	ver = (struct intel_version *)skb->data;
	if (ver->status) {
		BT_ERR("%s Intel fw version event failed (%02x)", hdev->name,
		       ver->status);
		kfree_skb(skb);
		return -bt_to_errno(ver->status);
	}

	BT_INFO("%s: read Intel version: %02x%02x%02x%02x%02x%02x%02x%02x%02x",
		hdev->name, ver->hw_platform, ver->hw_variant,
		ver->hw_revision, ver->fw_variant,  ver->fw_revision,
		ver->fw_build_num, ver->fw_build_ww, ver->fw_build_yy,
		ver->fw_patch_num);

	/* fw_patch_num indicates the version of patch the device currently
	 * have. If there is no patch data in the device, it is always 0x00.
	 * So, if it is other than 0x00, no need to patch the deivce again.
	 */
	if (ver->fw_patch_num) {
		BT_INFO("%s: Intel device is already patched. patch num: %02x",
			hdev->name, ver->fw_patch_num);
		kfree_skb(skb);
		btusb_check_bdaddr_intel(hdev);
		return 0;
	}

	/* Opens the firmware patch file based on the firmware version read
	 * from the controller. If it fails to open the matching firmware
	 * patch file, it tries to open the default firmware patch file.
	 * If no patch file is found, allow the device to operate without
	 * a patch.
	 */
	fw = btusb_setup_intel_get_fw(hdev, ver);
	if (!fw) {
		kfree_skb(skb);
		btusb_check_bdaddr_intel(hdev);
		return 0;
	}
	fw_ptr = fw->data;

	/* This Intel specific command enables the manufacturer mode of the
	 * controller.
	 *
	 * Only while this mode is enabled, the driver can download the
	 * firmware patch data and configuration parameters.
	 */
	skb = __hci_cmd_sync(hdev, 0xfc11, 2, mfg_enable, HCI_INIT_TIMEOUT);
	if (IS_ERR(skb)) {
		BT_ERR("%s entering Intel manufacturer mode failed (%ld)",
		       hdev->name, PTR_ERR(skb));
		release_firmware(fw);
		return PTR_ERR(skb);
	}

	if (skb->data[0]) {
		u8 evt_status = skb->data[0];

		BT_ERR("%s enable Intel manufacturer mode event failed (%02x)",
		       hdev->name, evt_status);
		kfree_skb(skb);
		release_firmware(fw);
		return -bt_to_errno(evt_status);
	}
	kfree_skb(skb);

	disable_patch = 1;

	/* The firmware data file consists of list of Intel specific HCI
	 * commands and its expected events. The first byte indicates the
	 * type of the message, either HCI command or HCI event.
	 *
	 * It reads the command and its expected event from the firmware file,
	 * and send to the controller. Once __hci_cmd_sync_ev() returns,
	 * the returned event is compared with the event read from the firmware
	 * file and it will continue until all the messages are downloaded to
	 * the controller.
	 *
	 * Once the firmware patching is completed successfully,
	 * the manufacturer mode is disabled with reset and activating the
	 * downloaded patch.
	 *
	 * If the firmware patching fails, the manufacturer mode is
	 * disabled with reset and deactivating the patch.
	 *
	 * If the default patch file is used, no reset is done when disabling
	 * the manufacturer.
	 */
	while (fw->size > fw_ptr - fw->data) {
		int ret;

		ret = btusb_setup_intel_patching(hdev, fw, &fw_ptr,
						 &disable_patch);
		if (ret < 0)
			goto exit_mfg_deactivate;
	}

	release_firmware(fw);

	if (disable_patch)
		goto exit_mfg_disable;

	/* Patching completed successfully and disable the manufacturer mode
	 * with reset and activate the downloaded firmware patches.
	 */
	skb = __hci_cmd_sync(hdev, 0xfc11, sizeof(mfg_reset_activate),
			     mfg_reset_activate, HCI_INIT_TIMEOUT);
	if (IS_ERR(skb)) {
		BT_ERR("%s exiting Intel manufacturer mode failed (%ld)",
		       hdev->name, PTR_ERR(skb));
		return PTR_ERR(skb);
	}
	kfree_skb(skb);

	BT_INFO("%s: Intel Bluetooth firmware patch completed and activated",
		hdev->name);

	btusb_check_bdaddr_intel(hdev);
	return 0;

exit_mfg_disable:
	/* Disable the manufacturer mode without reset */
	skb = __hci_cmd_sync(hdev, 0xfc11, sizeof(mfg_disable), mfg_disable,
			     HCI_INIT_TIMEOUT);
	if (IS_ERR(skb)) {
		BT_ERR("%s exiting Intel manufacturer mode failed (%ld)",
		       hdev->name, PTR_ERR(skb));
		return PTR_ERR(skb);
	}
	kfree_skb(skb);

	BT_INFO("%s: Intel Bluetooth firmware patch completed", hdev->name);

	btusb_check_bdaddr_intel(hdev);
	return 0;

exit_mfg_deactivate:
	release_firmware(fw);

	/* Patching failed. Disable the manufacturer mode with reset and
	 * deactivate the downloaded firmware patches.
	 */
	skb = __hci_cmd_sync(hdev, 0xfc11, sizeof(mfg_reset_deactivate),
			     mfg_reset_deactivate, HCI_INIT_TIMEOUT);
	if (IS_ERR(skb)) {
		BT_ERR("%s exiting Intel manufacturer mode failed (%ld)",
		       hdev->name, PTR_ERR(skb));
		return PTR_ERR(skb);
	}
	kfree_skb(skb);

	BT_INFO("%s: Intel Bluetooth firmware patch completed and deactivated",
		hdev->name);

	btusb_check_bdaddr_intel(hdev);
	return 0;
}

static int inject_cmd_complete(struct hci_dev *hdev, __u16 opcode)
{
	struct sk_buff *skb;
	struct hci_event_hdr *hdr;
	struct hci_ev_cmd_complete *evt;

	skb = bt_skb_alloc(sizeof(*hdr) + sizeof(*evt) + 1, GFP_ATOMIC);
	if (!skb)
		return -ENOMEM;

	hdr = (struct hci_event_hdr *)skb_put(skb, sizeof(*hdr));
	hdr->evt = HCI_EV_CMD_COMPLETE;
	hdr->plen = sizeof(*evt) + 1;

	evt = (struct hci_ev_cmd_complete *)skb_put(skb, sizeof(*evt));
	evt->ncmd = 0x01;
	evt->opcode = cpu_to_le16(opcode);

	*skb_put(skb, 1) = 0x00;

	bt_cb(skb)->pkt_type = HCI_EVENT_PKT;

	return hci_recv_frame(hdev, skb);
}

static int btusb_recv_bulk_intel(struct btusb_data *data, void *buffer,
				 int count)
{
	/* When the device is in bootloader mode, then it can send
	 * events via the bulk endpoint. These events are treated the
	 * same way as the ones received from the interrupt endpoint.
	 */
	if (test_bit(BTUSB_BOOTLOADER, &data->flags))
		return btusb_recv_intr(data, buffer, count);

	return btusb_recv_bulk(data, buffer, count);
}

static int btusb_recv_event_intel(struct hci_dev *hdev, struct sk_buff *skb)
{
	struct btusb_data *data = hci_get_drvdata(hdev);

	if (test_bit(BTUSB_BOOTLOADER, &data->flags)) {
		struct hci_event_hdr *hdr = (void *)skb->data;

		/* When the firmware loading completes the device sends
		 * out a vendor specific event indicating the result of
		 * the firmware loading.
		 */
		if (skb->len == 7 && hdr->evt == 0xff && hdr->plen == 0x05 &&
		    skb->data[2] == 0x06) {
			if (skb->data[3] != 0x00)
				test_bit(BTUSB_FIRMWARE_FAILED, &data->flags);

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(3, 18, 0))
			if (test_and_clear_bit(BTUSB_DOWNLOADING,
					       &data->flags) &&
			    test_bit(BTUSB_FIRMWARE_LOADED, &data->flags)) {
				smp_mb__after_atomic();
				wake_up_bit(&data->flags, BTUSB_DOWNLOADING);
			}
#else
			if (test_and_clear_bit(BTUSB_DOWNLOADING,
					       &data->flags) &&
			    test_bit(BTUSB_FIRMWARE_LOADED, &data->flags))
				wake_up_interruptible(&hdev->req_wait_q);
#endif
		}

		/* When switching to the operational firmware the device
		 * sends a vendor specific event indicating that the bootup
		 * completed.
		 */
		if (skb->len == 9 && hdr->evt == 0xff && hdr->plen == 0x07 &&
		    skb->data[2] == 0x02) {
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(3, 16, 0))
			if (test_and_clear_bit(BTUSB_BOOTING, &data->flags)) {
				smp_mb__after_atomic();
				wake_up_bit(&data->flags, BTUSB_BOOTING);
			}
#else
			if (test_and_clear_bit(BTUSB_BOOTING,
					       &data->flags) &&
			    test_bit(BTUSB_FIRMWARE_LOADED, &data->flags))
				wake_up_interruptible(&hdev->req_wait_q);
#endif
		}
	}

	return hci_recv_frame(hdev, skb);
}

static int btusb_send_frame_intel(struct hci_dev *hdev, struct sk_buff *skb)
{
	struct btusb_data *data = hci_get_drvdata(hdev);
	struct urb *urb;

	BT_DBG("%s", hdev->name);

	if (!test_bit(HCI_RUNNING, &hdev->flags))
		return -EBUSY;

	switch (bt_cb(skb)->pkt_type) {
	case HCI_COMMAND_PKT:
		if (test_bit(BTUSB_BOOTLOADER, &data->flags)) {
			struct hci_command_hdr *cmd = (void *)skb->data;
			__u16 opcode = le16_to_cpu(cmd->opcode);

			/* When in bootloader mode and the command 0xfc09
			 * is received, it needs to be send down the
			 * bulk endpoint. So allocate a bulk URB instead.
			 */
			if (opcode == 0xfc09)
				urb = alloc_bulk_urb(hdev, skb);
			else
				urb = alloc_ctrl_urb(hdev, skb);

			/* When the 0xfc01 command is issued to boot into
			 * the operational firmware, it will actually not
			 * send a command complete event. To keep the flow
			 * control working inject that event here.
			 */
			if (opcode == 0xfc01)
				inject_cmd_complete(hdev, opcode);
		} else {
			urb = alloc_ctrl_urb(hdev, skb);
		}
		if (IS_ERR(urb))
			return PTR_ERR(urb);

		hdev->stat.cmd_tx++;
		return submit_or_queue_tx_urb(hdev, urb);

	case HCI_ACLDATA_PKT:
		urb = alloc_bulk_urb(hdev, skb);
		if (IS_ERR(urb))
			return PTR_ERR(urb);

		hdev->stat.acl_tx++;
		return submit_or_queue_tx_urb(hdev, urb);

	case HCI_SCODATA_PKT:
		if (hci_conn_num(hdev, SCO_LINK) < 1)
			return -ENODEV;

		urb = alloc_isoc_urb(hdev, skb);
		if (IS_ERR(urb))
			return PTR_ERR(urb);

		hdev->stat.sco_tx++;
		return submit_tx_urb(hdev, urb);
	}

	return -EILSEQ;
}

static int btusb_intel_secure_send(struct hci_dev *hdev, u8 fragment_type,
				   u32 plen, const void *param)
{
	while (plen > 0) {
		struct sk_buff *skb;
		u8 cmd_param[253], fragment_len = (plen > 252) ? 252 : plen;

		cmd_param[0] = fragment_type;
		memcpy(cmd_param + 1, param, fragment_len);

		skb = __hci_cmd_sync(hdev, 0xfc09, fragment_len + 1,
				     cmd_param, HCI_INIT_TIMEOUT);
		if (IS_ERR(skb))
			return PTR_ERR(skb);

		kfree_skb(skb);

		plen -= fragment_len;
		param += fragment_len;
	}

	return 0;
}

static void btusb_intel_version_info(struct hci_dev *hdev,
				     struct intel_version *ver)
{
	const char *variant;

	switch (ver->fw_variant) {
	case 0x06:
		variant = "Bootloader";
		break;
	case 0x23:
		variant = "Firmware";
		break;
	default:
		return;
	}

	BT_INFO("%s: %s revision %u.%u build %u week %u %u", hdev->name,
		variant, ver->fw_revision >> 4, ver->fw_revision & 0x0f,
		ver->fw_build_num, ver->fw_build_ww, 2000 + ver->fw_build_yy);
}

static int btusb_setup_intel_new(struct hci_dev *hdev)
{
	static const u8 reset_param[] = { 0x00, 0x01, 0x00, 0x01,
					  0x00, 0x08, 0x04, 0x00 };
	struct btusb_data *data = hci_get_drvdata(hdev);
	struct sk_buff *skb;
	struct intel_version *ver;
	struct intel_boot_params *params;
	const struct firmware *fw;
	const u8 *fw_ptr;
	char fwname[64];
	ktime_t calltime, delta, rettime;
	unsigned long long duration;
	int err;

	BT_DBG("%s", hdev->name);

	calltime = ktime_get();

	/* Read the Intel version information to determine if the device
	 * is in bootloader mode or if it already has operational firmware
	 * loaded.
	 */
	skb = __hci_cmd_sync(hdev, 0xfc05, 0, NULL, HCI_INIT_TIMEOUT);
	if (IS_ERR(skb)) {
		BT_ERR("%s: Reading Intel version information failed (%ld)",
		       hdev->name, PTR_ERR(skb));
		return PTR_ERR(skb);
	}

	if (skb->len != sizeof(*ver)) {
		BT_ERR("%s: Intel version event size mismatch", hdev->name);
		kfree_skb(skb);
		return -EILSEQ;
	}

	ver = (struct intel_version *)skb->data;
	if (ver->status) {
		BT_ERR("%s: Intel version command failure (%02x)",
		       hdev->name, ver->status);
		err = -bt_to_errno(ver->status);
		kfree_skb(skb);
		return err;
	}

	/* The hardware platform number has a fixed value of 0x37 and
	 * for now only accept this single value.
	 */
	if (ver->hw_platform != 0x37) {
		BT_ERR("%s: Unsupported Intel hardware platform (%u)",
		       hdev->name, ver->hw_platform);
		kfree_skb(skb);
		return -EINVAL;
	}

	/* At the moment only the hardware variant iBT 3.0 (LnP/SfP) is
	 * supported by this firmware loading method. This check has been
	 * put in place to ensure correct forward compatibility options
	 * when newer hardware variants come along.
	 */
	if (ver->hw_variant != 0x0b) {
		BT_ERR("%s: Unsupported Intel hardware variant (%u)",
		       hdev->name, ver->hw_variant);
		kfree_skb(skb);
		return -EINVAL;
	}

	btusb_intel_version_info(hdev, ver);

	/* The firmware variant determines if the device is in bootloader
	 * mode or is running operational firmware. The value 0x06 identifies
	 * the bootloader and the value 0x23 identifies the operational
	 * firmware.
	 *
	 * When the operational firmware is already present, then only
	 * the check for valid Bluetooth device address is needed. This
	 * determines if the device will be added as configured or
	 * unconfigured controller.
	 *
	 * It is not possible to use the Secure Boot Parameters in this
	 * case since that command is only available in bootloader mode.
	 */
	if (ver->fw_variant == 0x23) {
		kfree_skb(skb);
		clear_bit(BTUSB_BOOTLOADER, &data->flags);
		btusb_check_bdaddr_intel(hdev);
		return 0;
	}

	/* If the device is not in bootloader mode, then the only possible
	 * choice is to return an error and abort the device initialization.
	 */
	if (ver->fw_variant != 0x06) {
		BT_ERR("%s: Unsupported Intel firmware variant (%u)",
		       hdev->name, ver->fw_variant);
		kfree_skb(skb);
		return -ENODEV;
	}

	kfree_skb(skb);

	/* Read the secure boot parameters to identify the operating
	 * details of the bootloader.
	 */
	skb = __hci_cmd_sync(hdev, 0xfc0d, 0, NULL, HCI_INIT_TIMEOUT);
	if (IS_ERR(skb)) {
		BT_ERR("%s: Reading Intel boot parameters failed (%ld)",
		       hdev->name, PTR_ERR(skb));
		return PTR_ERR(skb);
	}

	if (skb->len != sizeof(*params)) {
		BT_ERR("%s: Intel boot parameters size mismatch", hdev->name);
		kfree_skb(skb);
		return -EILSEQ;
	}

	params = (struct intel_boot_params *)skb->data;
	if (params->status) {
		BT_ERR("%s: Intel boot parameters command failure (%02x)",
		       hdev->name, params->status);
		err = -bt_to_errno(params->status);
		kfree_skb(skb);
		return err;
	}

	BT_INFO("%s: Device revision is %u", hdev->name,
		le16_to_cpu(params->dev_revid));

	BT_INFO("%s: Secure boot is %s", hdev->name,
		params->secure_boot ? "enabled" : "disabled");

	BT_INFO("%s: Minimum firmware build %u week %u %u", hdev->name,
		params->min_fw_build_nn, params->min_fw_build_cw,
		2000 + params->min_fw_build_yy);

	/* It is required that every single firmware fragment is acknowledged
	 * with a command complete event. If the boot parameters indicate
	 * that this bootloader does not send them, then abort the setup.
	 */
	if (params->limited_cce != 0x00) {
		BT_ERR("%s: Unsupported Intel firmware loading method (%u)",
		       hdev->name, params->limited_cce);
		kfree_skb(skb);
		return -EINVAL;
	}

	/* If the OTP has no valid Bluetooth device address, then there will
	 * also be no valid address for the operational firmware.
	 */
	if (!bacmp(&params->otp_bdaddr, BDADDR_ANY)) {
		BT_INFO("%s: No device address configured", hdev->name);
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(3, 17, 0))
		set_bit(HCI_QUIRK_INVALID_BDADDR, &hdev->quirks);
#endif
	}

	/* With this Intel bootloader only the hardware variant and device
	 * revision information are used to select the right firmware.
	 *
	 * Currently this bootloader support is limited to hardware variant
	 * iBT 3.0 (LnP/SfP) which is identified by the value 11 (0x0b).
	 */
	snprintf(fwname, sizeof(fwname), "intel/ibt-11-%u.sfi",
		 le16_to_cpu(params->dev_revid));

	err = request_firmware(&fw, fwname, &hdev->dev);
	if (err < 0) {
		BT_ERR("%s: Failed to load Intel firmware file (%d)",
		       hdev->name, err);
		kfree_skb(skb);
		return err;
	}

	BT_INFO("%s: Found device firmware: %s", hdev->name, fwname);

	kfree_skb(skb);

	if (fw->size < 644) {
		BT_ERR("%s: Invalid size of firmware file (%zu)",
		       hdev->name, fw->size);
		err = -EBADF;
		goto done;
	}

	set_bit(BTUSB_DOWNLOADING, &data->flags);

	/* Start the firmware download transaction with the Init fragment
	 * represented by the 128 bytes of CSS header.
	 */
	err = btusb_intel_secure_send(hdev, 0x00, 128, fw->data);
	if (err < 0) {
		BT_ERR("%s: Failed to send firmware header (%d)",
		       hdev->name, err);
		goto done;
	}

	/* Send the 256 bytes of public key information from the firmware
	 * as the PKey fragment.
	 */
	err = btusb_intel_secure_send(hdev, 0x03, 256, fw->data + 128);
	if (err < 0) {
		BT_ERR("%s: Failed to send firmware public key (%d)",
		       hdev->name, err);
		goto done;
	}

	/* Send the 256 bytes of signature information from the firmware
	 * as the Sign fragment.
	 */
	err = btusb_intel_secure_send(hdev, 0x02, 256, fw->data + 388);
	if (err < 0) {
		BT_ERR("%s: Failed to send firmware signature (%d)",
		       hdev->name, err);
		goto done;
	}

	fw_ptr = fw->data + 644;

	while (fw_ptr - fw->data < fw->size) {
		struct hci_command_hdr *cmd = (void *)fw_ptr;
		u8 cmd_len;

		cmd_len = sizeof(*cmd) + cmd->plen;

		/* Send each command from the firmware data buffer as
		 * a single Data fragment.
		 */
		err = btusb_intel_secure_send(hdev, 0x01, cmd_len, fw_ptr);
		if (err < 0) {
			BT_ERR("%s: Failed to send firmware data (%d)",
			       hdev->name, err);
			goto done;
		}

		fw_ptr += cmd_len;
	}

	set_bit(BTUSB_FIRMWARE_LOADED, &data->flags);

	BT_INFO("%s: Waiting for firmware download to complete", hdev->name);

	/* Before switching the device into operational mode and with that
	 * booting the loaded firmware, wait for the bootloader notification
	 * that all fragments have been successfully received.
	 *
	 * When the event processing receives the notification, then the
	 * BTUSB_DOWNLOADING flag will be cleared.
	 *
	 * The firmware loading should not take longer than 5 seconds
	 * and thus just timeout if that happens and fail the setup
	 * of this device.
	 */
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(3, 18, 0))
	err = btusb_wait_on_bit_timeout(&data->flags, BTUSB_DOWNLOADING,
					msecs_to_jiffies(5000),
					TASK_INTERRUPTIBLE);
	if (err == 1) {
		BT_ERR("%s: Firmware loading interrupted", hdev->name);
		err = -EINTR;
		goto done;
	}

	if (err) {
		BT_ERR("%s: Firmware loading timeout", hdev->name);
		err = -ETIMEDOUT;
		goto done;
	}
#else
	if (test_bit(BTUSB_DOWNLOADING, &data->flags)) {
		DECLARE_WAITQUEUE(wait, current);
		signed long timeout;

		BT_INFO("%s: Waiting for firmware download to complete",
			hdev->name);

		add_wait_queue(&hdev->req_wait_q, &wait);
		set_current_state(TASK_INTERRUPTIBLE);

		/* The firmware loading should not take longer than 5 seconds
		 * and thus just timeout if that happens and fail the setup
		 * of this device.
		 */
		timeout = schedule_timeout(msecs_to_jiffies(5000));

		remove_wait_queue(&hdev->req_wait_q, &wait);

		if (signal_pending(current)) {
			BT_ERR("%s: Firmware loading interrupted", hdev->name);
			err = -EINTR;
			goto done;
		}

		if (!timeout) {
			BT_ERR("%s: Firmware loading timeout", hdev->name);
			err = -ETIMEDOUT;
			goto done;
		}
	}
#endif
	if (test_bit(BTUSB_FIRMWARE_FAILED, &data->flags)) {
		BT_ERR("%s: Firmware loading failed", hdev->name);
		err = -ENOEXEC;
		goto done;
	}

	rettime = ktime_get();
	delta = ktime_sub(rettime, calltime);
	duration = (unsigned long long) ktime_to_ns(delta) >> 10;

	BT_INFO("%s: Firmware loaded in %llu usecs", hdev->name, duration);

done:
	release_firmware(fw);

	if (err < 0)
		return err;

	calltime = ktime_get();

	set_bit(BTUSB_BOOTING, &data->flags);

	skb = __hci_cmd_sync(hdev, 0xfc01, sizeof(reset_param), reset_param,
			     HCI_INIT_TIMEOUT);
	if (IS_ERR(skb))
		return PTR_ERR(skb);

	kfree_skb(skb);

	/* The bootloader will not indicate when the device is ready. This
	 * is done by the operational firmware sending bootup notification.
	 *
	 * Booting into operational firmware should not take longer than
	 * 1 second. However if that happens, then just fail the setup
	 * since something went wrong.
	 */
	BT_INFO("%s: Waiting for device to boot", hdev->name);

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(3, 18, 0))
	err = btusb_wait_on_bit_timeout(&data->flags, BTUSB_BOOTING,
					msecs_to_jiffies(1000),
					TASK_INTERRUPTIBLE);

	if (err == 1) {
		BT_ERR("%s: Device boot interrupted", hdev->name);
		return -EINTR;
	}

	if (err) {
		BT_ERR("%s: Device boot timeout", hdev->name);
		return -ETIMEDOUT;
	}
#else
	if (test_bit(BTUSB_BOOTING, &data->flags)) {
		DECLARE_WAITQUEUE(wait, current);
		signed long timeout;

		BT_INFO("%s: Waiting for firmware download to complete",
			hdev->name);

		add_wait_queue(&hdev->req_wait_q, &wait);
		set_current_state(TASK_INTERRUPTIBLE);

		/* The firmware loading should not take longer than 5 seconds
		 * and thus just timeout if that happens and fail the setup
		 * of this device.
		 */
		timeout = schedule_timeout(msecs_to_jiffies(5000));

		remove_wait_queue(&hdev->req_wait_q, &wait);

		if (signal_pending(current)) {
			BT_ERR("%s: Device boot interrupted", hdev->name);
			err = -EINTR;
			goto done;
		}
		if (!timeout) {
			BT_ERR("%s: Firmware loading timeout", hdev->name);
			err = -ETIMEDOUT;
			goto done;
		}
	}
#endif
	rettime = ktime_get();
	delta = ktime_sub(rettime, calltime);
	duration = (unsigned long long) ktime_to_ns(delta) >> 10;

	BT_INFO("%s: Device booted in %llu usecs", hdev->name, duration);

	clear_bit(BTUSB_BOOTLOADER, &data->flags);

	return 0;
}

#if (LINUX_VERSION_CODE > KERNEL_VERSION(3, 20, 0))
static void btusb_hw_error_intel(struct hci_dev *hdev, u8 code)
{
	struct sk_buff *skb;
	u8 type = 0x00;

	BT_ERR("%s: Hardware error 0x%2.2x", hdev->name, code);

	skb = __hci_cmd_sync(hdev, HCI_OP_RESET, 0, NULL, HCI_INIT_TIMEOUT);
	if (IS_ERR(skb)) {
		BT_ERR("%s: Reset after hardware error failed (%ld)",
		       hdev->name, PTR_ERR(skb));
		return;
	}
	kfree_skb(skb);

	skb = __hci_cmd_sync(hdev, 0xfc22, 1, &type, HCI_INIT_TIMEOUT);
	if (IS_ERR(skb)) {
		BT_ERR("%s: Retrieving Intel exception info failed (%ld)",
		       hdev->name, PTR_ERR(skb));
		return;
	}

	if (skb->len != 13) {
		BT_ERR("%s: Exception info size mismatch", hdev->name);
		kfree_skb(skb);
		return;
	}

	if (skb->data[0] != 0x00) {
		BT_ERR("%s: Exception info command failure (%02x)",
		       hdev->name, skb->data[0]);
		kfree_skb(skb);
		return;
	}

	BT_ERR("%s: Exception info %s", hdev->name, (char *)(skb->data + 1));

	kfree_skb(skb);
}
#endif

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(3, 17, 0))
static int btusb_set_bdaddr_intel(struct hci_dev *hdev, const bdaddr_t *bdaddr)
{
	struct sk_buff *skb;
	long ret;

	skb = __hci_cmd_sync(hdev, 0xfc31, 6, bdaddr, HCI_INIT_TIMEOUT);
	if (IS_ERR(skb)) {
		ret = PTR_ERR(skb);
		BT_ERR("%s: changing Intel device address failed (%ld)",
		       hdev->name, ret);
		return ret;
	}
	kfree_skb(skb);

	return 0;
}

static int btusb_set_bdaddr_marvell(struct hci_dev *hdev,
				    const bdaddr_t *bdaddr)
{
	struct sk_buff *skb;
	u8 buf[8];
	long ret;

	buf[0] = 0xfe;
	buf[1] = sizeof(bdaddr_t);
	memcpy(buf + 2, bdaddr, sizeof(bdaddr_t));

	skb = __hci_cmd_sync(hdev, 0xfc22, sizeof(buf), buf, HCI_INIT_TIMEOUT);
	if (IS_ERR(skb)) {
		ret = PTR_ERR(skb);
		BT_ERR("%s: changing Marvell device address failed (%ld)",
		       hdev->name, ret);
		return ret;
	}
	kfree_skb(skb);

	return 0;
}
#endif

#define BDADDR_BCM20702A0 (&(bdaddr_t) {{0x00, 0xa0, 0x02, 0x70, 0x20, 0x00}})

static int btusb_setup_bcm_patchram(struct hci_dev *hdev)
{
	struct btusb_data *data = hci_get_drvdata(hdev);
	struct usb_device *udev = data->udev;
	char fw_name[64];
	const struct firmware *fw;
	const u8 *fw_ptr;
	size_t fw_size;
	const struct hci_command_hdr *cmd;
	const u8 *cmd_param;
	u16 opcode;
	struct sk_buff *skb;
	struct hci_rp_read_local_version *ver;
	struct hci_rp_read_bd_addr *bda;
	long ret;

	snprintf(fw_name, sizeof(fw_name), "brcm/%s-%04x-%04x.hcd",
		 udev->product ? udev->product : "BCM",
		 le16_to_cpu(udev->descriptor.idVendor),
		 le16_to_cpu(udev->descriptor.idProduct));

	ret = request_firmware(&fw, fw_name, &hdev->dev);
	if (ret < 0) {
		BT_INFO("%s: BCM: patch %s not found", hdev->name, fw_name);
		return 0;
	}

	/* Reset */
	skb = __hci_cmd_sync(hdev, HCI_OP_RESET, 0, NULL, HCI_INIT_TIMEOUT);
	if (IS_ERR(skb)) {
		ret = PTR_ERR(skb);
		BT_ERR("%s: HCI_OP_RESET failed (%ld)", hdev->name, ret);
		goto done;
	}
	kfree_skb(skb);

	/* Read Local Version Info */
	skb = btusb_read_local_version(hdev);
	if (IS_ERR(skb)) {
		ret = PTR_ERR(skb);
		goto done;
	}

	ver = (struct hci_rp_read_local_version *)skb->data;
	BT_INFO("%s: BCM: apply patch", hdev->name);
	kfree_skb(skb);

	/* Start Download */
	skb = __hci_cmd_sync(hdev, 0xfc2e, 0, NULL, HCI_INIT_TIMEOUT);
	if (IS_ERR(skb)) {
		ret = PTR_ERR(skb);
		BT_ERR("%s: BCM: Download Minidrv command failed (%ld)",
		       hdev->name, ret);
		goto reset_fw;
	}
	kfree_skb(skb);

	/* 50 msec delay after Download Minidrv completes */
	msleep(50);

	fw_ptr = fw->data;
	fw_size = fw->size;

	while (fw_size >= sizeof(*cmd)) {
		cmd = (struct hci_command_hdr *)fw_ptr;
		fw_ptr += sizeof(*cmd);
		fw_size -= sizeof(*cmd);

		if (fw_size < cmd->plen) {
			BT_ERR("%s: BCM: patch %s is corrupted",
			       hdev->name, fw_name);
			ret = -EINVAL;
			goto reset_fw;
		}

		cmd_param = fw_ptr;
		fw_ptr += cmd->plen;
		fw_size -= cmd->plen;

		opcode = le16_to_cpu(cmd->opcode);

		skb = __hci_cmd_sync(hdev, opcode, cmd->plen, cmd_param,
				     HCI_INIT_TIMEOUT);
		if (IS_ERR(skb)) {
			ret = PTR_ERR(skb);
			BT_ERR("%s: BCM: patch command %04x failed (%ld)",
			       hdev->name, opcode, ret);
			goto reset_fw;
		}
		kfree_skb(skb);
	}

	/* 250 msec delay after Launch Ram completes */
	msleep(250);

reset_fw:
	/* Reset */
	skb = __hci_cmd_sync(hdev, HCI_OP_RESET, 0, NULL, HCI_INIT_TIMEOUT);
	if (IS_ERR(skb)) {
		ret = PTR_ERR(skb);
		BT_ERR("%s: HCI_OP_RESET failed (%ld)", hdev->name, ret);
		goto done;
	}
	kfree_skb(skb);

	/* Read Local Version Info */
	skb = btusb_read_local_version(hdev);
	if (IS_ERR(skb)) {
		ret = PTR_ERR(skb);
		goto done;
	}

	kfree_skb(skb);

	/* Read BD Address */
	skb = __hci_cmd_sync(hdev, HCI_OP_READ_BD_ADDR, 0, NULL,
			     HCI_INIT_TIMEOUT);
	if (IS_ERR(skb)) {
		ret = PTR_ERR(skb);
		BT_ERR("%s: HCI_OP_READ_BD_ADDR failed (%ld)",
		       hdev->name, ret);
		goto done;
	}

	if (skb->len != sizeof(*bda)) {
		BT_ERR("%s: HCI_OP_READ_BD_ADDR event length mismatch",
		       hdev->name);
		kfree_skb(skb);
		ret = -EIO;
		goto done;
	}

	bda = (struct hci_rp_read_bd_addr *)skb->data;
	if (bda->status) {
		BT_ERR("%s: HCI_OP_READ_BD_ADDR error status (%02x)",
		       hdev->name, bda->status);
		kfree_skb(skb);
		ret = -bt_to_errno(bda->status);
		goto done;
	}

	/* The address 00:20:70:02:A0:00 indicates a BCM20702A0 controller
	 * with no configured address.
	 */
	if (!bacmp(&bda->bdaddr, BDADDR_BCM20702A0)) {
		BT_INFO("%s: BCM: using default device address (%pMR)",
			hdev->name, &bda->bdaddr);
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(3, 17, 0))
		set_bit(HCI_QUIRK_INVALID_BDADDR, &hdev->quirks);
#endif
	}

	kfree_skb(skb);

done:
	release_firmware(fw);

	return ret;
}

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(3, 17, 0))
static int btusb_set_bdaddr_bcm(struct hci_dev *hdev, const bdaddr_t *bdaddr)
{
	struct sk_buff *skb;
	long ret;

	skb = __hci_cmd_sync(hdev, 0xfc01, 6, bdaddr, HCI_INIT_TIMEOUT);
	if (IS_ERR(skb)) {
		ret = PTR_ERR(skb);
		BT_ERR("%s: BCM: Change address command failed (%ld)",
		       hdev->name, ret);
		return ret;
	}
	kfree_skb(skb);

	return 0;
}

static int btusb_set_bdaddr_ath3012(struct hci_dev *hdev,
				    const bdaddr_t *bdaddr)
{
	struct sk_buff *skb;
	u8 buf[10];
	long ret;

	buf[0] = 0x01;
	buf[1] = 0x01;
	buf[2] = 0x00;
	buf[3] = sizeof(bdaddr_t);
	memcpy(buf + 4, bdaddr, sizeof(bdaddr_t));

	skb = __hci_cmd_sync(hdev, 0xfc0b, sizeof(buf), buf, HCI_INIT_TIMEOUT);
	if (IS_ERR(skb)) {
		ret = PTR_ERR(skb);
		BT_ERR("%s: Change address command failed (%ld)",
		       hdev->name, ret);
		return ret;
	}
	kfree_skb(skb);

	return 0;
}
#endif

static int btusb_probe(struct usb_interface *intf,
		       const struct usb_device_id *id)
{
	struct usb_endpoint_descriptor *ep_desc;
	struct btusb_data *data;
	struct hci_dev *hdev;
	int i, err;

	BT_DBG("intf %p id %p", intf, id);

	/* interface numbers are hardcoded in the spec */
	if (intf->cur_altsetting->desc.bInterfaceNumber != 0)
		return -ENODEV;

	if (!id->driver_info) {
		const struct usb_device_id *match;

		match = usb_match_id(intf, blacklist_table);
		if (match)
			id = match;
	}

	if (id->driver_info == BTUSB_IGNORE)
		return -ENODEV;

	if (id->driver_info & BTUSB_ATH3012) {
		struct usb_device *udev = interface_to_usbdev(intf);

		/* Old firmware would otherwise let ath3k driver load
		 * patch and sysconfig files */
		if (le16_to_cpu(udev->descriptor.bcdDevice) <= 0x0001)
			return -ENODEV;
	}

	data = devm_kzalloc(&intf->dev, sizeof(*data), GFP_KERNEL);
	if (!data)
		return -ENOMEM;

	for (i = 0; i < intf->cur_altsetting->desc.bNumEndpoints; i++) {
		ep_desc = &intf->cur_altsetting->endpoint[i].desc;

		if (!data->intr_ep && usb_endpoint_is_int_in(ep_desc)) {
			data->intr_ep = ep_desc;
			continue;
		}

		if (!data->bulk_tx_ep && usb_endpoint_is_bulk_out(ep_desc)) {
			data->bulk_tx_ep = ep_desc;
			continue;
		}

		if (!data->bulk_rx_ep && usb_endpoint_is_bulk_in(ep_desc)) {
			data->bulk_rx_ep = ep_desc;
			continue;
		}
	}

	if (!data->intr_ep || !data->bulk_tx_ep || !data->bulk_rx_ep)
		return -ENODEV;

	if (id->driver_info & BTUSB_AMP) {
		data->cmdreq_type = USB_TYPE_CLASS | 0x01;
		data->cmdreq = 0x2b;
	} else {
		data->cmdreq_type = USB_TYPE_CLASS;
		data->cmdreq = 0x00;
	}

	data->udev = interface_to_usbdev(intf);
	data->intf = intf;

	INIT_WORK(&data->work, btusb_work);
	INIT_WORK(&data->waker, btusb_waker);
	init_usb_anchor(&data->deferred);
	init_usb_anchor(&data->tx_anchor);
	spin_lock_init(&data->txlock);

	init_usb_anchor(&data->intr_anchor);
	init_usb_anchor(&data->bulk_anchor);
	init_usb_anchor(&data->isoc_anchor);
	spin_lock_init(&data->rxlock);

	if (id->driver_info & BTUSB_INTEL_NEW) {
		data->recv_event = btusb_recv_event_intel;
		data->recv_bulk = btusb_recv_bulk_intel;
		set_bit(BTUSB_BOOTLOADER, &data->flags);
	} else {
		data->recv_event = hci_recv_frame;
		data->recv_bulk = btusb_recv_bulk;
	}

	hdev = hci_alloc_dev();
	if (!hdev)
		return -ENOMEM;

	hdev->bus = HCI_USB;
	hci_set_drvdata(hdev, data);

	if (id->driver_info & BTUSB_AMP)
		hdev->dev_type = HCI_AMP;
	else
		hdev->dev_type = HCI_BREDR;

	data->hdev = hdev;

	SET_HCIDEV_DEV(hdev, &intf->dev);

	hdev->open   = btusb_open;
	hdev->close  = btusb_close;
	hdev->flush  = btusb_flush;
	hdev->send   = btusb_send_frame;
	hdev->notify = btusb_notify;

	if (id->driver_info & BTUSB_BCM92035)
		hdev->setup = btusb_setup_bcm92035;

	if (id->driver_info & BTUSB_BCM_PATCHRAM) {
		hdev->setup = btusb_setup_bcm_patchram;
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(3, 17, 0))
		hdev->set_bdaddr = btusb_set_bdaddr_bcm;
#endif
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(3, 19, 0))
		set_bit(HCI_QUIRK_STRICT_DUPLICATE_FILTER, &hdev->quirks);
#endif
	}

	if (id->driver_info & BTUSB_RTL8723A) {
		hdev->setup = btusb_setup_rtl8723a;

		/* Realtek devices lose their updated firmware over suspend,
		 * but the USB hub doesn't notice any status change.
		 * Explicitly request a device reset on resume.
		 */
		set_bit(BTUSB_RESET_RESUME, &data->flags);
	}
	if (id->driver_info & BTUSB_RTL8723B) {
		hdev->setup = btusb_setup_rtl8723b;
		set_bit(BTUSB_RESET_RESUME, &data->flags);
	}
	if (id->driver_info & BTUSB_INTEL) {
		hdev->setup = btusb_setup_intel;
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(3, 17, 0))
		hdev->set_bdaddr = btusb_set_bdaddr_intel;
#endif
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(3, 19, 0))
		set_bit(HCI_QUIRK_STRICT_DUPLICATE_FILTER, &hdev->quirks);
#endif
	}

	if (id->driver_info & BTUSB_INTEL_NEW) {
		hdev->send = btusb_send_frame_intel;
		hdev->setup = btusb_setup_intel_new;
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(3, 20, 0))
		hdev->hw_error = btusb_hw_error_intel;
		set_bit(HCI_QUIRK_STRICT_DUPLICATE_FILTER, &hdev->quirks);
#endif
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(3, 17, 0))
		hdev->set_bdaddr = btusb_set_bdaddr_intel;
#endif
	}

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(3, 17, 0))
	if (id->driver_info & BTUSB_MARVELL)
		hdev->set_bdaddr = btusb_set_bdaddr_marvell;
#endif

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(3, 20, 0))
	if (id->driver_info & BTUSB_SWAVE) {
		set_bit(HCI_QUIRK_FIXUP_INQUIRY_MODE, &hdev->quirks);
		set_bit(HCI_QUIRK_BROKEN_LOCAL_COMMANDS, &hdev->quirks);
	}
#endif

	if (id->driver_info & BTUSB_INTEL_BOOT)
		set_bit(HCI_QUIRK_RAW_DEVICE, &hdev->quirks);

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(3, 17, 0))
	if (id->driver_info & BTUSB_ATH3012) {
		hdev->set_bdaddr = btusb_set_bdaddr_ath3012;
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(3, 19, 0))
		set_bit(HCI_QUIRK_STRICT_DUPLICATE_FILTER, &hdev->quirks);
#endif
	}
#endif

	if (id->driver_info & BTUSB_AMP) {
		/* AMP controllers do not support SCO packets */
		data->isoc = NULL;
	} else {
		/* Interface numbers are hardcoded in the specification */
		data->isoc = usb_ifnum_to_if(data->udev, 1);
	}

	if (!reset)
		set_bit(HCI_QUIRK_RESET_ON_CLOSE, &hdev->quirks);

	if (force_scofix || id->driver_info & BTUSB_WRONG_SCO_MTU) {
		if (!disable_scofix)
			set_bit(HCI_QUIRK_FIXUP_BUFFER_SIZE, &hdev->quirks);
	}

	if (id->driver_info & BTUSB_BROKEN_ISOC)
		data->isoc = NULL;

	if (id->driver_info & BTUSB_DIGIANSWER) {
		data->cmdreq_type = USB_TYPE_VENDOR;
		set_bit(HCI_QUIRK_RESET_ON_CLOSE, &hdev->quirks);
	}

	if (id->driver_info & BTUSB_CSR) {
		struct usb_device *udev = data->udev;
		u16 bcdDevice = le16_to_cpu(udev->descriptor.bcdDevice);

		/* Old firmware would otherwise execute USB reset */
		if (bcdDevice < 0x117)
			set_bit(HCI_QUIRK_RESET_ON_CLOSE, &hdev->quirks);

		/* Fake CSR devices with broken commands */
		if (bcdDevice <= 0x100)
			hdev->setup = btusb_setup_csr;
	}

	if (id->driver_info & BTUSB_SNIFFER) {
		struct usb_device *udev = data->udev;

		/* New sniffer firmware has crippled HCI interface */
		if (le16_to_cpu(udev->descriptor.bcdDevice) > 0x997)
			set_bit(HCI_QUIRK_RAW_DEVICE, &hdev->quirks);
	}

	if (id->driver_info & BTUSB_INTEL_BOOT) {
		/* A bug in the bootloader causes that interrupt interface is
		 * only enabled after receiving SetInterface(0, AltSetting=0).
		 */
		err = usb_set_interface(data->udev, 0, 0);
		if (err < 0) {
			BT_ERR("failed to set interface 0, alt 0 %d", err);
			hci_free_dev(hdev);
			return err;
		}
	}

	if (data->isoc) {
		err = usb_driver_claim_interface(&btusb_driver,
						 data->isoc, data);
		if (err < 0) {
			hci_free_dev(hdev);
			return err;
		}
	}

	err = hci_register_dev(hdev);
	if (err < 0) {
		hci_free_dev(hdev);
		return err;
	}

	usb_set_intfdata(intf, data);

	return 0;
}

static void btusb_disconnect(struct usb_interface *intf)
{
	struct btusb_data *data = usb_get_intfdata(intf);
	struct hci_dev *hdev;

	BT_DBG("intf %p", intf);

	if (!data)
		return;

	hdev = data->hdev;
	usb_set_intfdata(data->intf, NULL);

	if (data->isoc)
		usb_set_intfdata(data->isoc, NULL);

	hci_unregister_dev(hdev);

	if (intf == data->isoc)
		usb_driver_release_interface(&btusb_driver, data->intf);
	else if (data->isoc)
		usb_driver_release_interface(&btusb_driver, data->isoc);

	hci_free_dev(hdev);
}

#ifdef CONFIG_PM
static int btusb_suspend(struct usb_interface *intf, pm_message_t message)
{
	struct btusb_data *data = usb_get_intfdata(intf);

	BT_DBG("intf %p", intf);

	if (data->suspend_count++)
		return 0;

	spin_lock_irq(&data->txlock);
	if (!(PMSG_IS_AUTO(message) && data->tx_in_flight)) {
		set_bit(BTUSB_SUSPENDING, &data->flags);
		spin_unlock_irq(&data->txlock);
	} else {
		spin_unlock_irq(&data->txlock);
		data->suspend_count--;
		return -EBUSY;
	}

	cancel_work_sync(&data->work);

	btusb_stop_traffic(data);
	usb_kill_anchored_urbs(&data->tx_anchor);

	/* Optionally request a device reset on resume, but only when
	 * wakeups are disabled. If wakeups are enabled we assume the
	 * device will stay powered up throughout suspend.
	 */
	if (test_bit(BTUSB_RESET_RESUME, &data->flags) &&
	    !device_may_wakeup(&data->udev->dev))
		data->udev->reset_resume = 1;

	return 0;
}

static void play_deferred(struct btusb_data *data)
{
	struct urb *urb;
	int err;

	while ((urb = usb_get_from_anchor(&data->deferred))) {
		err = usb_submit_urb(urb, GFP_ATOMIC);
		if (err < 0)
			break;

		data->tx_in_flight++;
	}
	usb_scuttle_anchored_urbs(&data->deferred);
}

static int btusb_resume(struct usb_interface *intf)
{
	struct btusb_data *data = usb_get_intfdata(intf);
	struct hci_dev *hdev = data->hdev;
	int err = 0;

	BT_DBG("intf %p", intf);

	if (--data->suspend_count)
		return 0;

	if (!test_bit(HCI_RUNNING, &hdev->flags))
		goto done;

	if (test_bit(BTUSB_INTR_RUNNING, &data->flags)) {
		err = btusb_submit_intr_urb(hdev, GFP_NOIO);
		if (err < 0) {
			clear_bit(BTUSB_INTR_RUNNING, &data->flags);
			goto failed;
		}
	}

	if (test_bit(BTUSB_BULK_RUNNING, &data->flags)) {
		err = btusb_submit_bulk_urb(hdev, GFP_NOIO);
		if (err < 0) {
			clear_bit(BTUSB_BULK_RUNNING, &data->flags);
			goto failed;
		}

		btusb_submit_bulk_urb(hdev, GFP_NOIO);
	}

	if (test_bit(BTUSB_ISOC_RUNNING, &data->flags)) {
		if (btusb_submit_isoc_urb(hdev, GFP_NOIO) < 0)
			clear_bit(BTUSB_ISOC_RUNNING, &data->flags);
		else
			btusb_submit_isoc_urb(hdev, GFP_NOIO);
	}

	spin_lock_irq(&data->txlock);
	play_deferred(data);
	clear_bit(BTUSB_SUSPENDING, &data->flags);
	spin_unlock_irq(&data->txlock);
	schedule_work(&data->work);

	return 0;

failed:
	usb_scuttle_anchored_urbs(&data->deferred);
done:
	spin_lock_irq(&data->txlock);
	clear_bit(BTUSB_SUSPENDING, &data->flags);
	spin_unlock_irq(&data->txlock);

	return err;
}
#endif

static struct usb_driver btusb_driver = {
	.name		= "btusb",
	.probe		= btusb_probe,
	.disconnect	= btusb_disconnect,
#ifdef CONFIG_PM
	.suspend	= btusb_suspend,
	.resume		= btusb_resume,
#endif
	.id_table	= btusb_table,
	.supports_autosuspend = 1,
	.disable_hub_initiated_lpm = 1,
};

module_usb_driver(btusb_driver);

module_param(disable_scofix, bool, 0644);
MODULE_PARM_DESC(disable_scofix, "Disable fixup of wrong SCO buffer size");

module_param(force_scofix, bool, 0644);
MODULE_PARM_DESC(force_scofix, "Force fixup of wrong SCO buffers size");

module_param(reset, bool, 0644);
MODULE_PARM_DESC(reset, "Send HCI reset command on initialization");

MODULE_AUTHOR("Marcel Holtmann <marcel@holtmann.org>");
MODULE_DESCRIPTION("Generic Bluetooth USB driver ver " VERSION);
MODULE_VERSION(VERSION);
MODULE_LICENSE("GPL");
