# Wi-Fi (BCM4329)

Wi-Fi works with WPA2-PSK. It was verified on one phone, one access point
(WPA2, AES/CCMP, 2.4 GHz, about -44 dBm) and one firmware, with the example
app `bravo_hello`. Everything below was observed on the device unless it is
marked *inference*.

## What the system provides

* `wpa_supplicant` 2.12 with **only the wext driver** (`nerves_defconfig`:
  `BR2_PACKAGE_WPA_SUPPLICANT=y`, `..._WEXT=y`, `..._CTRL_IFACE=y`,
  `..._PASSPHRASE=y`, nl80211 off). Buildroot enables wext by default only for
  toolchains *without* threads, so with musl it has to be switched on
  explicitly. Binaries: `/usr/sbin/wpa_supplicant`, `/usr/sbin/wpa_passphrase`.
* BusyBox `udhcpc` (`/sbin/udhcpc`) and `ifconfig`/`route`.
* `/usr/libexec/bravo-udhcpc`, a minimal udhcpc event script (see
  [DHCP](#dhcp-the-stock-script-hangs)). It is in this system's
  `rootfs_overlay/`; a system built before it was added does not contain it, in
  which case the firmware must ship it (`bravo_hello` does).

The system does **not** start any of this. Your application starts the driver,
`wpa_supplicant` and `udhcpc` (reference: `bravo_hello`
`BravoHello.Hw.Wifi` and `BravoHello.Net.Wifi`).

## Proprietary files (never commit or publish them)

Copy them from your own phone, whose Android `/system` partition this system
never touches (mtd3). `bravo_hello/scripts/fetch-wifi-blobs` does it over SSH
(mounting `/system` read-only):

| File on the phone | Put it at (in the firmware's rootfs overlay) |
|---|---|
| `/system/lib/modules/bcm4329.ko` | `/lib/modules/bcm4329.ko` |
| `/system/vendor/firmware/fw_bcm4329.bin` | `/lib/firmware/fw_bcm4329.bin` |

The module's vermagic matches the stock kernel, so it loads there.

## Bring-up, step by step

1. **Load the driver** (creates `wlan0`, takes a few seconds):
   ```sh
   insmod /lib/modules/bcm4329.ko firmware_path=/lib/firmware/fw_bcm4329.bin iface_name=wlan
   ifconfig wlan0 up
   ```
2. **Scan with the private ioctl, before associating.** The driver is built with
   `CSCAN`, so the standard scan call (`SIOCSIWSCAN`) fails with `EINVAL`.
   Scans use `SIOCSIWPRIV`: the string `START` once, then
   `CSCAN S\x01\x00\x00S\x00` (a broadcast scan); results are read with the
   normal `SIOCGIWSCAN`. See `bravo_hello/c_src/wscan.c`.
3. **Start wpa_supplicant** with `-Dwext -i wlan0 -c /root/wpa.conf`. The
   config must use `ap_scan=2` (below). It must not contain the passphrase if
   you can avoid it: store the derived 256-bit PSK instead
   (`wpa_passphrase`, or PBKDF2-HMAC-SHA1 with 4096 rounds and the SSID as salt;
   `bravo_hello/scripts/bravo-config wifi` does this).
   ```
   ctrl_interface=/tmp/wpa_supplicant
   update_config=0
   ap_scan=2
   country=NL
   network={
     ssid="YourSSID"
     key_mgmt=WPA-PSK
     proto=RSN
     pairwise=CCMP
     group=CCMP
     psk=<64 hex digits>
   }
   ```
4. **Start DHCP** with the bundled script:
   `udhcpc -i wlan0 -f -R -s /usr/libexec/bravo-udhcpc`.
5. Keep scanning (step 2) every ~10 s while there is no IP address, so the
   driver's scan list stays fresh for the next association attempt.

Order that worked from a cold boot: load driver, scan, start wpa_supplicant,
start udhcpc. The IP address arrived within about a minute of boot.

## Why it needs all that (what went wrong first)

* **`CTRL-EVENT-SCAN-FAILED ret=-1`, repeating.** wpa_supplicant's wext scan
  uses the standard `SIOCSIWSCAN`, which this driver rejects. Fix:
  `ap_scan=2`. wpa_supplicant then never scans or selects an AP; it only sets
  the SSID and keys, and the driver scans and joins by itself
  (`Trying to associate with SSID`, `Associated with <bssid>`).
* **`WPA: Could not find AP from the scan results`, then
  `4-Way Handshake failed - pre-shared key may be incorrect` /
  `reason=WRONG_KEY`.** Misleading: the key was right (message 3's MIC verified
  and decrypted). With `ap_scan=2` wpa_supplicant still wants the AP's
  RSN element from the driver's scan list to validate message 3, and that list
  is empty unless something scanned. A private-ioctl scan (step 2) right before
  associating fills it; the handshake then completes
  (`Key negotiation completed [PTK=CCMP GTK=CCMP]`).
* **Explicit `proto/pairwise/group`.** With `ap_scan=2` wpa_supplicant cannot read
  the AP's cipher suites, so the config must say them. `RSN` + `CCMP` is WPA2-AES.
  A TKIP-only or WPA/WPA2 mixed-mode AP needs different values
  (*inference*, not tested).
* **VintageNetWiFi cannot be used** (it needs nl80211 and a scannable
  interface). Supervise `wpa_supplicant` and `udhcpc` yourself, restarting them
  when they exit; `bravo_hello` keeps them as Erlang ports and kills them by
  OS pid (closing a port does not stop the process).

## DHCP: the stock script hangs

`udhcpc` ran, but the interface never got an address. The stock
`/usr/share/udhcpc/default.script` was stuck in its `deconfig` step, with a
`cat` child waiting on stdin. *Inference:* the script does
`TMPFILE=$(mktemp)`, `mktemp` produced nothing on this BusyBox, and
`cat $TMPFILE > /etc/resolv.conf` with an empty name reads stdin forever.
`/usr/libexec/bravo-udhcpc` only runs `ifconfig` (address, netmask, broadcast)
and `route` (default gateway), and deliberately does not write
`/etc/resolv.conf` (no DNS needed for LAN use; avoids rewriting NAND on each
lease renewal). If you need DNS, write it to a tmpfs path.

Also worth knowing about this BusyBox (checked on the device): `tr` and `head`
are not installed, and `ifconfig` has no status display ("no support for status
display"), so `ifconfig wlan0` cannot show the address. Read it from
`:inet.getifaddrs/0` or `/proc/net/*`.

## Status information

* Signal: `/proc/net/wireless` (`level` column, dBm).
* Link events: wpa_supplicant's stdout (`CTRL-EVENT-CONNECTED`, ...). Only
  `-dd` shows the EAPOL exchange; keys are not printed without `-K`.
* The kernel log is not needed for any of the above.

## SSH over Wi-Fi

NervesSSH listens on all interfaces, so once `wlan0` has an address you can
`ssh` to the phone over the LAN with the same key; the USB cable is no longer
required (verified, including `bravo-deploy --host <ip>`). This also exposes
the SSH port to everyone on that network (key authentication only). Restrict it
or put the phone on a separate network if that matters.

## Limits

* WPA2-PSK (AES) only; no WPA3, no enterprise EAP (`BR2_PACKAGE_WPA_SUPPLICANT_EAP`
  is off), no AP mode, no roaming handling.
* No power-save tuning, no regulatory domain handling (the driver was not
  asked for one).
* Tested with one access point. Different security settings may need different
  `wpa.conf` values.
* The driver and firmware are vendor files: not distributed here.
