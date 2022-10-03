dir ../os161-1.11/kern/compile/ASST2
target remote unix:.sockets/gdb
break panic
break sys_execv
break as_define_stack
break runprogram
break cmd_progthread
c
