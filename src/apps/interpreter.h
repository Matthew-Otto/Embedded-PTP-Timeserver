#ifndef INTERPRETER_H
#define INTERPRETER_H

typedef void (*cmd_func_t)(int argc, char **argv);

typedef struct {
    const char *name;
    cmd_func_t func;
    const char *help;
} cmd_t;

void interpreter(void);

#endif // INTERPRETER_H