#### This is the kernel loader

#### This code should be stored in the first sector of a hard disk
#### (first 512 byte). The BIOS load this code to physical address
#### 0x7c00-0x7e00 (512 byte) while it runs and jumps to the beginning
#### of it.
#### In real mode, the loader loads the kernel into memory and jumps
#### to its entry point, which is the start function in start.s

#### The BIOS passes in the drive that the loader was read from as DL,
#### with floppy drives numbered 0x00, 0x01, ... and hard drives 
#### numbered from 0x80, 0x81, ... We want to support booting a
#### kernel on different drive from the loader, we don't use that.

## The real mode segment is of 16-bit
    .code16

## setup segment registers.
## as stack grows downward, we set it from 60KB downwardly (after
## boot, the kernel continues to use this stack for its initial threads).

    sub %ax, %ax        # set ax, ds, ss to 0
    mov %ax, %ds
    mov %ax, %ss
    mov $0xf000, %esp   # set esp to 60K

## configure serial port so we can report progress without
## connected VGA. See[IntrList] for detail.
    
    sub %dx, %dx        # set dx to 0, serial port 0
    mov $0xe3, %al      # 9600 bps, N-8-1, AL store the serial interrupt
                        # initialize parameter in the form of BBBPPSLL
                        # more info can be check from https://courses.
                        # engr.illinois.edu/ece390/books/labmanual/
                        # serial-comm-bios.html
    int $0x14           # 0x14 interrupt to initialize port as AH
                        # has already be 0

    call puts
    .string "LoLo"
#### Read the MBR partition table on hard disk and scan for a partition
#### of type 0x20, which is the type we use for our kernel.

#### We print out status message and partition and disk being scanned.
#### e.g. hda1234 as we scan four partitions on the first hard disk.

    mov $0x80, %dl
read_mbr:
    sub %ebx, %ebx      # Sector 0
    mov $0x2000, %ax    # use 8K for buffer
    mov %ax, %es
    call read_sector
    jc no_such_drive    # jc for jump if carry

    # Print hd[a-z]
    call puts
    .string " hd"
    mov %dl, %al
    add $'a' - 0x80, %al
    call putc           # putc uses a parameter in %al

    # check for MBR signature -- if not present, it's not a
    # partitioned hard disk.

    cmpw $0xaa55, %es:510 #0xaa55 is the end signature of MBR
    jne next_drive

    mov $446, %si
    mov $'1', %al
check_partition:
    # Is it an unused partition?
    cmpl $0, %es:(%si)
    je next_partition
    
    # Print [1-4].
    call putc

    # Is it a kernel partition?
    cmpb $0x20, %es:4(%si)
    jne next_partition

    # Is it a bootable partition?
    cmpb $0x80, %es:(%si)
    je load_kernel

next_partition:
    # no match for this partition, go on next
    add $16, %si    # Offset to next partition table entry.
    inc %al
    cmp $510, %si
    jb check_partition

next_drive:
    # No match on this drive, go on to the next one.
    inc %dl
    jnc read_mbr

no_such_drive:
no_boot_partition:
    # Didn't find a kernel partition in the end, give up
    call puts
    .string "\rNot found\r"

    # Notify BIOS that boot failed.
    int $0x18       # Execute Cassette BASIC or display no BASIC found

#### We found a kernel whoes drive is in DL. The partition table entry
#### for the kernel's partition is at ES:SI. Our job now is to read the
#### kernel from disk and jump to its start address.

load_kernel:
    call puts
    .string "\rLoading"

    # Find out number of sectors to read. the kernel is just an ELF
    # format object. which is not easy to read and easy to find out
    # its own size. We limit the kernel to 512KB for some other reason
    # So it's enought to read the entire content of partition or 512KB
    # from disk, even it is smaller.
    mov %es:12(%si), %ecx   # EBP = number of sectors
    cmp $1024, %ecx         # Cap size at 512K
    jbe 1f
    mov $1024, %cx
1:
    mov %es:8(%si), %ebx    # EBX = first sector
    mov $0x2000, %ax        # Start load address: 0x2000

next_sector:
    # Read one sector into memory.
    mov %ax, %es            # ES:0000 -> load address
    call read_sector
    jc read_failed

    # Print '.' as progress indicator once every 16 sectors == 8KB.
    test $15, %bl
    jnz 1f
    call puts
    .string "."
1:
    
    # Advance memory pointer and disk sector.
    add $0x20, %ax
    inc %bx
    loop next_sector

    call puts
    .string "\r"

#### Transfer control to the kernel. We read the start address out of
#### the ELF header and convert it from a 32-bit linear address into 
#### a 16:16 segment:offset address for real mode, then jump to the
#### converted address. The 80x86 doesn't have an instruction to jump
#### to an absolute segment:offset kept in register, so in fact we store
#### the address in a temporary memory location, then jump indirectly
#### through that location. To save 4 bytes in the loader, we reuse 4
#### bytes of the loader's code for this temporary pointer.

    mov $0x2000, %ax
    mov %ax, %es
    mov %es:0x18, %dx
    mov %dx, start
    movw $0x2000, start + 2
    ljmp *start

read_failed:
start:
    # Disk sector read failed.
    call puts
1:  .string "\rBad read\r"

    # Notify BIOS that boot failed.
    int 0x18

#### Print string subroutine. To save space in the loader, this
#### subroutine takes tis null-terminated string argument from the code
#### stream right after the call, and then returns to the byte right
#### after the terminating null. This subroutine preserves all general-
#### purpose registers.

puts: xchg %si, %ss:(%esp)
    push %ax
next_char:
    mov %cs:(%si),%al
    inc %si
    test %al, %al
    jz 1f
    call putc
    jmp next_char
1:  pop %ax
    xchg %si, %ss:(esp)
    ret

#### Chararcter output subroutine. Prints character in AL to VGA display
#### and serial port 0. using BIOS services. preserves all general-
#### purpose registers.

#### If called upon to output a carriage return, this subroutine 
#### automatically supplies the following line feed.

putc: pusha

1:  sub %bh, %bh        # Page 0.
    mov $0x0e, %ah      # Teletype output service
    int $0x10

    mov $0x01, %ah      # Serial port output service
    sub %dx, %dx        # Serial port 0
2:  int $0x14           # Destory AH
    test $0x80, %ah     # timeout ?
    jz 3f
    movw $0x9090, 2b    # Turn "int $0x14" above into NOPs.

3:  cmp $'\r', %al
    jne popa_ret
    mov $'\n', %al
    jmp 1b

#### Sector read subroutine. Takes a drive number in DL (0x80 = hard disk
#### 0, 0x81 = hard disk 1, ...) and a sector number in EBX, and reads
#### the specified secotr into memory at ES:0000. Returns with carray set
#### on error, clear otherwise. Preserves all general-purpose registers.

read_sector:
    pusha
    sub %ax, %ax
    push %ax            # LBA sector number [48:63]
    push %ax            # LBA sector number [32:47]
    push %ebx           # LBA sector number [0:31]
    push %es            # Buffer segment
    push %ax            # Buffer offset (always 0)
    push $1             # Number of sectors to read
    push $16            # Packet size
    mov $0x42, %ah      # Extended read
    mov %sp, %si        # DS:SI -> packet
    int $0x13           # error code in CF(carry flag)
    popa                # pop 16 bytes, preserve flags
popa_ret:
    popa
    ret                 # Error code still in CF

