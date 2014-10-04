#include <stdio.h>
#include <refu.h>

#include <compiler_args.h>
#include <front_ctx.h>
#include <Utils/log.h>

int main(int argc,char** argv)
{
    int rc = 0;
    struct compiler_args *args;
    struct front_ctx front;
    //initialize Refu library
    rf_init("refuclib.log", 0, LOG_DEBUG);

    //parse the arguments given to the compiler
    compiler_args_modinit();
    args = compiler_args_parse(argc, argv);
    if(!args) {
        ERROR("Failure at argument parsing");
        rc = 1;
        goto err;
    }

    if (rf_string_is_empty(&args->input)) {
        ERROR("No input file provided");
        rc = 1;
        goto err;
    }

    if (!front_ctx_init(&front, &args->input)) {
        ERROR("Failure at frontend initialization");
        rc = 1;
        goto err;
    }

    if (!front_ctx_process(&front)) {
        ERROR("Failure to parse the input");
        rc = 1;
        goto err;
    } else {
        printf("input file parsed succesfully");
    }

err:
    rf_deinit();
    return rc;
}

