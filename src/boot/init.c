#include "init.h"
#include <console.h>
#include <debug.h>
#include <intypes.h>
#include <limits.h>
#include <random.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "devices/kbd.h"
#include "devices/input.h"
#include "devices/serial.h"
#include "devices/shutdown.h"
#include "devices/vga.h"
#include "devices/timer.h"
#include "devices/rtc.h"
#include "core/interrupt.h"
#include "core/io.h"
#include "loader.h"
#include "mm/malloc.h"
#include "mm/palloc.h"
#include "core/pte.h"
#include "threads/thread.h"

#ifdef USERPROG
#include "user/process.h"
#include "user/exception.h"
#include "user/gdt.h"
#include "user/syscall.h"
#include "user/tss.h"
#else
#include "tests/threads/test.h"
#endif

#ifdef FILESYS
#include "devices/block.h"
#include "devices/ide.h"
#include "fs/fs.h"
#include "fs/fsutil.h"
#endif

/* Page directory with kernel mapping only. */
uint32_t *init_page_dir;

#ifdef FILESYS
/* -f: Format the file system. */
static bool format_filesys;

/* -filesys, -scratch, -swap; Name of block devices to use.
   overriding the defaults. */
static const char *filesys_bdev_name;
static const char *scratch_bdev_name;

#ifdef VM
static const char *swap_bdev_name;
#endif
#endif  /* END of FILESYS */

/* -ul: Maximum number of pages to put into palloc's user pool.  */
static size_t user_page_limit = SIZE_MAX;

static void bss_init(void);
static char** read_command_line(void);
static char** parse_options(char** argv);
static void paging_init(void);
static void run_actions(char ** argv);
static void usage(void);

#ifdef FILESYS
static void locate_block_devices(void);
static void locate_block_device(enum block_type, const char* name);
#endif

int
main(void)
{
    char **argv;

    /* Clear bss. Make bss all be zero. */
    bss_init();

    /* Break command-line into arguments and parse options. */
    argv = read_command_line();
    argv = parse_options(argv);

    /* Initialize init itself as a thread so that it can use locks,
       and then enable console locking. */
    thread_init();
    console_init();

    /* Welcome user using this system. */
    printf("SimpleOS booting with %'"PRIu32" KB RAM...\n",
            init_ram_pages * PGSIZE / 1024);

    /* Initialize memory system */
    palloc_init(user_page_limit);
    malloc_init();
    paging_init();

    /* Segmentation. */
#ifdef USERPROG
    tss_init();
    gdt_init();
#endif
    
    /* Initialize interrupt handlers. */
    intr_init();
    timer_init();
    kdb_init();
    input_init();

#ifdef USERPROG
    exception_init();
    syscall_init();
#endif

    /* Start thread scheduler and enable interrupts. */
    thread_start();
    serial_init_queue();
    timer_calibrate();

#ifdef FILESYS
    /* Initialize file system. */
    ide_init();
    locale_block_devices();
    fs_init(format_filesys);
#endif
    printf("Boot complete!\n");
    /* Run actions specified on kernel command line. */
    run_actions(argv);

    /* Finish up. */
    shutdown();
    thread_exit();

}


/* Clear BSS, BSS should be initialized to zeros. It isn't actually
   stored on disk or zeroed by kernel loader, so we have to zero it manually.
   
  The start and end of bss is recorded by the linker as _start_bss and _end_bss. */
static void
bss_init(void)
{
    extern char _start_bss, _end_bss;
    memset(&_start_bss, 0, _end_bss - _start_bss);
}


/* Populates the base page directory and page table with the kernel virtual
   mapping, and the sets up the CPU to use the new page directory. Points
   init_page_dir to the page dirctory it creates. */
static void
paging_init(void)
{
    uint32_t *pd, *pt;
    size_t page;
    extern char _start, _end_kernel_text;

    pd = init_page_dir = palloc_get_page(PAL_ASSERT | PAL_ZERO);
    pt = NULL;
    for (page = 0; page < init_ram_pages; page++) {
        uintptr_t paddr = page * PGSIZE;
        char *vaddr = ptov(paddr);      /* Convert Physical addr to Virtual addr. */
        size_t pde_idx = pd_no(vaddr);
        size_t ptd_idx = pt_no(vaddr);
        bool in_kernel_text = (&_start <= vaddr) && (vaddr < &_end_kernel_text);

        if (!pd[pde_idx]) {
            pt = palloc_get_page(PAL_ASSERT | PAL_ZERO);
            pd[pde_idx] = pde_create(pt);
        }

        pt[pte_idx] = pte_create_kernel(vaddr, !in_kernel_text);
    }

    /* Store the physcal address of the page dirctory into CR3 aka
       (also known as) PDBR(page dirctory base register). This activates
       our new page tables immediately. */
    asm volatile ("movl %0, %%cr3" : : "r" (vtop (init_page_dir)));

}


/* Breaks the kernel command line into wrds and returns them as an argv-like arry */
static char **
read_command_line(void)
{
    static *argv[LOADER_ARGS_LEN / 2 + 1];
    char *p, *end;
    int argc;
    int i;
    
    argc = *(uint32_t*)ptov(LOADER_ARG_CNT);
    p = ptov(LOADER_ARGS);
    end = p + LOADER_ARGS_LEN;
    for (i = 0; i < argc; i++) {
        if (p >= end)
            PANIC("command line overflow");
        argv[i] = p;
        p += strnlen(p, end - p) + 1;
    }
    /* "The end of argv is a NULL ptr, recall exec syscalls in Unix" */
    argv[argc] = NULL;

    /* Print kernel command line. */
    printf("Kernel command line:");
    for (i = 0; i < argc; i++)
        (!strchr(argv[i])) ? printf(" %s",argv[i]) : printf(" '%s'", argv[i]);
    printf("\n");
    return argv;
}

/* Parses options in ARGV[] and returns the first non-option argument. */
static char**
parse_options(char **argv)
{
    for (; *argv != NULL && **argv == '-'; argv++) {
        char *save_ptr;
        char *name = strtok_r(*arv, "=", &save_ptr);
        char *value = strtok_r(NULL, "", &save_ptr);

        if (!strcmp(name, "-h"))
            usage();
        else if (!strcmp(name, "-q"))
            shutdown_configure(SHUTDOWN_POWER_OFF);
        else if (!strcmp(name, "-r"))
            shutdown_configure(SHUTDOWN_REBOOT);
#ifdef FILESYS
        else if (!strcmp(name, "-f"))
            format_filesys = true;
        else if (!strcmp(name, "-filesys"))
            filesys_bdev_name = value;
        else if (!strcmp(name, "-scratch"))
            scratch_bdev_name = value;
#ifdef VM
        else if (!strcmp(name, "-swap"))
            swap_bdev_name = value;
#endif  /* END of VM */
#endif  /* END of FILESYS */

        else if (!strcmp(name, "-rs"))
            random_init(atoi(value));
        else if (!strcmp(name, "-mlfqs"))
            thread_mlfqs = true;
#ifdef USERPROG
        else if (!strcmp(name, "ul"))
            user_page_limit = atoi(value);
#endif  /* END of USERPROG */
        else
            PANIC("unknown option '%s' (use -h for help)", name);
    }

    /* Initialize the random number generator based on the system time. This 
       has no effect if an "-rs" option was specified
     
       When running under Boche, this is not enough by itself to get a good
       seed value, because the kernel script sets the initial time to a
       predictable value, not to the local time, for reproducibility. To fix
       this, give the "-r" option to the pintos script to request real-time
       execution. */

    random_init(rtc_get_time());
    return argv;
}


/* Prints a kernel command line help message and powers off the
   machine. */
static void
usage (void)
{
  printf ("\nCommand line syntax: [OPTION...] [ACTION...]\n"
          "Options must precede actions.\n"
          "Actions are executed in the order specified.\n"
          "\nAvailable actions:\n"
#ifdef USERPROG
          "  run 'PROG [ARG...]' Run PROG and wait for it to complete.\n"
#else
          "  run TEST           Run TEST.\n"
#endif
#ifdef FILESYS
          "  ls                 List files in the root directory.\n"
          "  cat FILE           Print FILE to the console.\n"
          "  rm FILE            Delete FILE.\n"
          "Use these actions indirectly via `pintos' -g and -p options:\n"
          "  extract            Untar from scratch device into file system.\n"
          "  append FILE        Append FILE to tar file on scratch device.\n"
#endif
          "\nOptions:\n"
          "  -h                 Print this help message and power off.\n"
          "  -q                 Power off VM after actions or on panic.\n"
          "  -r                 Reboot after actions.\n"
#ifdef FILESYS
          "  -f                 Format file system device during startup.\n"
          "  -filesys=BDEV      Use BDEV for file system instead of default.\n"
          "  -scratch=BDEV      Use BDEV for scratch instead of default.\n"
#ifdef VM
          "  -swap=BDEV         Use BDEV for swap instead of default.\n"
#endif
#endif
          "  -rs=SEED           Set random number seed to SEED.\n"
          "  -mlfqs             Use multi-level feedback queue scheduler.\n"
#ifdef USERPROG
          "  -ul=COUNT          Limit user memory to COUNT pages.\n"
#endif
          );
  shutdown_power_off ();
}


#ifdef FILESYS
/* Figure out what block devices to cast in the various kernel roles. */
static void
locate_block_devices(void)
{
    locate_block_device(BLOCK_FILESYS, filesys_bdev_name);
    locate_block_device(BLOCK_SCRATCH, scratch_bdev_name);
#ifdef VM
    locate_block_device(BLOCK_SWAP, swap_bdev_name);
#endif  /* END of VM */
}

/* Figures out what block device to use for the given ROLE: the block device
   with the given NAME, if NAME is non-null, otherwise the first block device
   in probe order of type ROLE. */
static void
locate_block_device(enum block_type role, const char* name)
{
    struct block* blk = NULL;
    if (name) {
        blk = block_get_by_name(name);
        if (!blk)
            PANIC("No such block device \"%s\"", name);
    } else {
        for (blk = block_first(); blk; blk = block_next(blk))
            if (block_type(blk) == role)
                break;
    }

    if (blk) {
        printf("%s: using %s\n", block_type_name(role), block_name(blk));
        block_set_role(role, blk);
    }
}

#endif  /* END of FILESYS */
