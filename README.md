# АК-2. Лабораторна робота 6 — модулі ядра Linux (частина 3)

Виконано **завдання 1**: модифікація модулів `hello1`/`hello2` з лабораторної роботи 5
для отримання двох видів аварійних повідомлень ядра (oops) та пошук місця аварії.

* **`hello2`** — замість `pr_err()` + `return -EINVAL` для `count > 10` викликається
  `BUG_ON(count > HELLO_MAX_COUNT)`.
* **`hello1`** — у `print_hello()` для виклику з номером `fail_at` (параметр модуля,
  за умовчанням 5, `0` — вимкнено) замість `kmalloc()` береться `NULL`, начебто
  `kmalloc()` повернув 0. Перевірки на `NULL` немає навмисно, тому запис
  `ev->start = ktime_get()` звертається за адресою `0 + offsetof(struct hello_event, start)`
  = `0x00000008` і ядро друкує oops «NULL pointer dereference».
* **`Makefile`** — як у appendix1: модулі збираються з `-g`, копія з налагоджувальною
  інформацією зберігається як `*.ko.unstripped`, а з `*.ko` вона видаляється
  (`strip -g`). Додано `make dis` (дизасемблювання з кодом, `objdump -dS`) і
  `make hello1.s` / `make hello1.i`.

## Файли

| Файл           | Призначення                                                        |
|----------------|--------------------------------------------------------------------|
| `inc/hello1.h` | спільний заголовок: `struct hello_event`, прототип `print_hello()` |
| `hello1.c`     | експортує `print_hello()`, список, параметр `fail_at` (внесення помилки) |
| `hello2.c`     | параметр `count`, `BUG_ON()` для `count > 10`                       |
| `Makefile`     | kbuild з `-g`, `*.ko.unstripped` + `strip -g`, `make dis`, `checkpatch` |

## Збирання

```bash
export ARCH=arm
export CROSS_COMPILE="ccache arm-linux-gnueabihf-"
export KDIR=$HOME/repos/linux-stable

make              # hello1.ko, hello2.ko (stripped) + *.ko.unstripped
make dis          # hello1.dis, hello2.dis
make checkpatch
modinfo hello1.ko # з'явився параметр fail_at
ls -l *.ko *.ko.unstripped
```

`CROSS_COMPILE` містить `ccache`, тому `strip`/`objdump` у Makefile викликаються
з префіксом `$(lastword $(CROSS_COMPILE))` = `arm-linux-gnueabihf-`.

Для пошуку місця аварії потрібен gdb, що розуміє ARM:

```bash
sudo apt install gdb-multiarch
```

## Запуск у QEMU

```bash
cp hello1.ko hello2.ko ~/repos/busybox/_install/root/
cd ~/repos/busybox/_install
find . | cpio -o -H newc | gzip > ../rootfs.cpio.gz
cd ..
qemu-system-arm -kernel _install/boot/zImage -initrd rootfs.cpio.gz \
    -machine virt -nographic -m 512 \
    --append "root=/dev/ram0 rw console=ttyAMA0,115200 mem=512M rdinit=/init"
```

Після oops модуль залишається в стані «завантажується» і його неможливо вивантажити,
тому кожен експеримент виконується після нового запуску QEMU (вихід: `Ctrl-A`, `X`).

### Експеримент 1 — `BUG_ON()`

```sh
cd /root
insmod hello1.ko
insmod hello2.ko count=3      # нормальна робота
rmmod hello2
insmod hello2.ko count=11     # kernel BUG at .../hello2.c:NN!
lsmod
rmmod hello2                  # не вдається: модуль "зайнятий"
rmmod hello1                  # теж не вдається: його використовує hello2
cat /sys/module/hello2/sections/.init.text
cat /sys/module/hello2/sections/.text
```

### Експеримент 2 — «kmalloc() повернув 0»

```sh
cd /root
insmod hello1.ko              # fail_at=5
cat /sys/module/hello1/parameters/fail_at
cat /sys/module/hello1/sections/.text
insmod hello2.ko count=3      # виклики 1..3 — успішно
rmmod hello2
insmod hello2.ko count=3      # виклик 5 — Unable to handle kernel NULL pointer dereference at virtual address 00000008
lsmod
```

## Пошук місця аварії (на хост-машині)

В oops є рядок `PC is at <функція>+<зміщення>/<розмір> [<модуль>]` та значення
регістра `pc : [<адреса>]`.

**Спосіб 1 — за символом і зміщенням** (зручно для обох oops):

```bash
gdb-multiarch -q hello1.ko.unstripped
(gdb) list *(print_hello+0x40)       # зміщення з рядка "PC is at"
(gdb) info line *(print_hello+0x40)
(gdb) disassemble /s print_hello

gdb-multiarch -q hello2.ko.unstripped
(gdb) list *(hello2_init+0x10)
```

**Спосіб 2 — за абсолютною адресою з oops та адресами секцій із sysfs**
(`/sys/module/<модуль>/sections/`, імена файлів починаються з крапки):

```bash
gdb-multiarch -q
(gdb) add-symbol-file hello1.ko.unstripped 0xbf000000          # = /sys/module/hello1/sections/.text
(gdb) list *0xbf000040                                          # = pc з oops
# для hello2_init (секція .init.text):
(gdb) add-symbol-file hello2.ko.unstripped -s .text 0xbf006000 -s .init.text 0xbf00a000
(gdb) list *0xbf00a010
```

**Спосіб 3 — objdump**: `make dis`, у `hello1.dis` знайти `<print_hello>:` і
додати зміщення з oops до адреси функції; поруч буде рядок коду C.

```bash
arm-linux-gnueabihf-objdump -dS hello1.ko.unstripped | less
```

Адреси вище — приклади, підставте значення зі свого виводу.
