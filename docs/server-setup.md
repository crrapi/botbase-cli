# server setup

install v2.5 if youre on hos < 21. on hos 21+ the memory got tight -
official v2.5 can crash at boot with 2001-0132 in am or hid. build PR
#111 (lowers static heap to 2MB, trims socket buffers, shows 2.6):

```sh
git clone https://github.com/olliz0r/sys-botbase
cd sys-botbase
curl https://github.com/quinnfrost/sys-botbase/commit/1eacdc6751ebfbee4cebae674dfce5856280c95e.patch | git apply
make -C sys-botbase    # needs devkitpro + libnx 4.10+
# sys-botbase.nsp -> atmosphere/contents/430000000000000B/exefs.nsp
```

if it still OOMs, disable sysmodules youre not using. see the extra
system memory note in the atmosphere source.

checking: sys-botbase keeps answering getVersion even during an
atmosphere fatal. a reply is not proof of a healthy boot. look in
atmosphere/crash_reports/ instead.
