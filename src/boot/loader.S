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
