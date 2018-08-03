## Prime + Probe is a last-level cache side-channel attack.

main.c is a attack code and key.c is a victim code.
the attacker knows the victim code and a platform information. (e.g. LLC structure)
note: run main.c and key.c on different cores. (xx an yy are different cores)

    taskset -c xx ./key.o

    taskset -c yy ./main.o

to check the result, 

    python draw.py