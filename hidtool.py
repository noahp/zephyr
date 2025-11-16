# /// script
# requires-python = ">=3.8"
# dependencies = [
#   "hidapi"
# ]
# ///

#!/usr/bin/env python3
"""
hidtool.py — HID IN/OUT/Feature reports with Report ID support.

Examples:
    python hidtool.py --list
    python hidtool.py --vid 0x1234 --pid 0x5678 --report-id 5 --out 01 02 03
    python hidtool.py --vid 0x1234 --pid 0x5678 --read 64
    python hidtool.py --vid 0x1234 --pid 0x5678 --listen 64
"""

import argparse
import sys
import time

import hid


def list_devices():
    devs = hid.enumerate()
    if not devs:
        print("No HID devices found.")
        return

    for d in devs:
        vid = d["vendor_id"]
        pid = d["product_id"]
        rel = d.get("release_number", 0)

        man = d.get("manufacturer_string") or ""
        prod = d.get("product_string") or ""
        ser = d.get("serial_number") or ""

        path = d["path"].decode() if isinstance(d["path"], bytes) else d["path"]
        iface = d.get("interface_number")

        print("------------------------------------------------------------")
        print(f"idVendor          0x{vid:04x} {man}")
        print(f"idProduct         0x{pid:04x} {prod}")
        print(f"bcdDevice          {rel >> 8}.{rel & 0xFF:02}")
        print(f"iManufacturer        {1 if man else 0} {man}")
        print(f"iProduct             {2 if prod else 0} {prod}")
        print(f"iSerial              {3 if ser else 0} {ser}")
        print()
        print(f"Path:              {path}")
        print(f"Interface:         {iface}")
        print(f"Usage Page:        {d.get('usage_page')}")
        print(f"Usage:             {d.get('usage')}")
        print()


def open_dev(vid, pid):
    dev = hid.device()
    dev.open(vid, pid)
    dev.set_nonblocking(True)
    return dev


def parse_hex_bytes(hex_list):
    return bytes(int(x, 16) for x in hex_list)


def pretty_report(data, raw=False):
    if not data:
        return "(no data)"
    if raw:
        return data.hex()
    rid = data[0]
    payload = data[1:]
    return f"RID {rid:02x} | {payload.hex()}"


def main():
    p = argparse.ArgumentParser()
    p.add_argument("--list", action="store_true", help="List HID devices")
    p.add_argument("--vid", type=lambda x: int(x, 16), help="Vendor ID (hex)")
    p.add_argument("--pid", type=lambda x: int(x, 16), help="Product ID (hex)")
    p.add_argument(
        "--report-id",
        type=lambda x: int(x, 16),
        help="Report ID for OUT writes (default 0)",
    )
    p.add_argument("--out", nargs="*", help="Payload bytes for OUT report")
    p.add_argument("--read", type=int, help="Read N bytes once")
    p.add_argument("--listen", type=int, help="Continuously read N bytes")
    p.add_argument(
        "--get-feature", type=lambda x: int(x, 0), help="Get feature report by ID"
    )
    p.add_argument(
        "--set-feature", nargs="*", help="Set feature report (payload bytes)"
    )
    p.add_argument("--raw", action="store_true", help="Show raw report bytes")
    args = p.parse_args()

    if args.list:
        list_devices()
        return

    if not args.vid or not args.pid:
        print("Error: --vid and --pid required")
        sys.exit(1)

    # try opening device- if this fails, show a snippet that needs to be added to udev rules
    try:
        dev = open_dev(args.vid, args.pid)
    except OSError as e:
        print(
            f"Error: Could not open device VID 0x{args.vid:04x} PID 0x{args.pid:04x}: {e}"
        )
        print("You may need to add a udev rule like the following:")
        print(
            f'echo \'KERNEL=="hidraw*", ATTRS{{idVendor}}=="{args.vid:04x}", ATTRS{{idProduct}}=="{args.pid:04x}", MODE="0666"\' | sudo tee /etc/udev/rules.d/99-myhid.rules >/dev/null && sudo udevadm control --reload-rules && sudo udevadm trigger'
        )
        raise

    # OUT report
    if args.out:
        rid = args.report_id if args.report_id is not None else 0
        payload = parse_hex_bytes(args.out)
        packet = bytes([rid]) + payload
        print("Sending:", packet.hex())
        dev.write(packet)

    # Single read
    if args.read:
        r = dev.read(args.read, timeout_ms=3000)
        print("Read:", pretty_report(bytes(r), raw=args.raw) if r else "(no data)")

    # Get feature report
    if args.get_feature is not None:
        try:
            r = dev.get_feature_report(args.get_feature, 256)  # Max 256 bytes
            if r:
                print(f"Feature Report {args.get_feature}:", bytes(r).hex())
                if not args.raw and len(r) > 0:
                    print(f"  Data: {bytes(r[1:]).hex()}")  # Skip report ID
                    # Try to interpret as string if printable
                    try:
                        data = bytes(r[1:])
                        # Remove null bytes and decode if it looks like text
                        text = data.rstrip(b"\x00").decode("utf-8", errors="ignore")
                        if text.isprintable() and len(text) > 0:
                            print(f"  Text: '{text}'")
                    except Exception:
                        pass
            else:
                print(f"Feature Report {args.get_feature}: (no data)")
        except Exception as e:
            print(f"Error getting feature report: {e}")

    # Set feature report
    if args.set_feature and args.report_id is not None:
        try:
            payload = parse_hex_bytes(args.set_feature)
            packet = bytes([args.report_id]) + payload
            print("Setting feature report:", packet.hex())
            result = dev.send_feature_report(packet)
            print(f"Feature report set, result: {result}")
        except Exception as e:
            print(f"Error setting feature report: {e}")

    # Continuous listen
    if args.listen:
        print("Listening... Ctrl-C to stop")
        try:
            while True:
                r = dev.read(args.listen)
                if r:
                    print(pretty_report(bytes(r), raw=args.raw))
                time.sleep(0.0005)
        except KeyboardInterrupt:
            pass


if __name__ == "__main__":
    main()
