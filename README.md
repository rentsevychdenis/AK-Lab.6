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
```

Секції `.text` у `hello2` немає: модуль містить лише `__init`- і `__exit`-функції
(секції `.init.text` і `.exit.text`).

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

Отримані oops (QEMU, ядро 7.3.0-rc6):

| oops | рядок `PC is at` | `pc` | адреса секції в sysfs |
|------|------------------|------|-----------------------|
| `BUG_ON()` | `hello2_init+0x18/0xf80 [hello2]` | `0xbf00c098` | `.init.text` = `0xbf00c080` |
| NULL pointer | `print_hello+0x60/0x140 [hello1]` | `0xbf000060` | `.text` = `0xbf000000` |

Команди gdb передаються через `-ex` у режимі `-batch` (вставлені в інтерактивний gdb
одним блоком, вони читаються як один рядок).

**Спосіб 1 — функція + зміщення з рядка `PC is at`:**

```bash
gdb-multiarch -q -batch -ex 'list *(print_hello+0x60)' \
    -ex 'info line *(print_hello+0x60)' hello1.ko.unstripped
gdb-multiarch -q -batch -ex 'list *(hello2_init+0x18)' hello2.ko.unstripped
```

**Спосіб 2 — абсолютна адреса `pc` + адреса секції з `/sys/module/<модуль>/sections/`**
(основний файл не передається, інакше gdb бере таблицю рядків із копії без зміщення):

```bash
gdb-multiarch -q -batch -ex 'add-symbol-file hello1.ko.unstripped 0xbf000000' \
    -ex 'list *0xbf000060' -ex 'info line *0xbf000060'
# hello2_init — у секції .init.text
gdb-multiarch -q -batch -ex 'add-symbol-file hello2.ko.unstripped -s .init.text 0xbf00c080' \
    -ex 'list *0xbf00c098' -ex 'info line *0xbf00c098'
```

**Спосіб 3 — objdump** (`make dis` або напряму):

```bash
arm-linux-gnueabihf-objdump -dS --disassemble=print_hello hello1.ko.unstripped | grep -B12 -A3 " 60:"
```

Результат: NULL pointer — `hello1.c:52` (`ev->start = ktime_get();`, інструкція
`strd r2, r3, [r4, #8]`, r4 = NULL); `BUG_ON()` — `hello2.c:37` (інструкція `udf`, `e7f001f2`).
