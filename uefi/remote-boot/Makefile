ARCH := x86_64
EFIINC ?= /usr/include/efi
EFILIB ?= /usr/lib
EFIGNULIB ?= $(EFILIB)
CRT0 ?= $(EFILIB)/crt0-efi-x86_64.o
LDS ?= $(EFILIB)/elf_x86_64_efi.lds
CFLAGS := -I$(EFIINC) -I$(EFIINC)/$(ARCH) -fpic -ffreestanding -fno-stack-protector -fno-stack-check -fshort-wchar -mno-red-zone -maccumulate-outgoing-args -DEFI_FUNCTION_WRAPPER -Wall -Wextra -Werror -O2
all: RemoteBoot.efi
remote_boot.o: remote_boot.c load_option.h
	$(CC) $(CFLAGS) -c $< -o $@
remote_boot.so: remote_boot.o
	$(LD) -nostdlib -znocombreloc -T $(LDS) -shared -Bsymbolic $(CRT0) $< -L$(EFILIB) -L$(EFIGNULIB) -lefi -lgnuefi -o $@
RemoteBoot.efi: remote_boot.so
	objcopy -j .text -j .sdata -j .data -j .rodata -j .dynamic -j .dynsym -j .rel -j .rela -j '.rel.*' -j '.rela.*' -j .reloc --output-target=efi-app-x86_64 $< $@
clean:
	rm -f remote_boot.o remote_boot.so RemoteBoot.efi
.PHONY: all clean
