#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <inttypes.h>

#include "wasm_c_api.h"

#define own

// A function to be called from Wasm code.
own wasm_trap_t* fail_callback(
  void* env, const wasm_val_t args[], wasm_val_t results[]
) {
  printf("Calling back...\n");
  own wasm_name_t message;
  wasm_name_new_from_string_nt(&message, "callback abort");
  own wasm_trap_t* trap = wasm_trap_new((wasm_store_t*)env, &message);
  wasm_name_delete(&message);
  return trap;
}

int main(int argc, const char* argv[]) {
  // Initialize.
  printf("Initializing...\n");
  wasm_engine_t* engine = wasm_engine_new();
  wasm_store_t* store = wasm_store_new(engine);

  // Load binary.
  printf("Loading binary...\n");
#if WASM_ENABLE_AOT != 0 && WASM_ENABLE_INTERP == 0
  FILE* file = fopen("trap.aot", "rb");
#else
  FILE* file = fopen("trap.wasm", "rb");
#endif
  if (!file) {
    printf("> Error loading module!\n");
    return 1;
  }
  fseek(file, 0L, SEEK_END);
  size_t file_size = ftell(file);
  fseek(file, 0L, SEEK_SET);
  wasm_byte_vec_t binary;
  wasm_byte_vec_new_uninitialized(&binary, file_size);
  if (fread(binary.data, file_size, 1, file) != 1) {
    printf("> Error loading module!\n");
    return 1;
  }
  fclose(file);

  // Compile.
  printf("Compiling module...\n");
  own wasm_module_t* module = wasm_module_new(store, &binary);
  if (!module) {
    printf("> Error compiling module!\n");
    return 1;
  }

  wasm_byte_vec_delete(&binary);

  // Create external print functions.
  printf("Creating callback...\n");
  own wasm_functype_t* fail_type =
    wasm_functype_new_0_1(wasm_valtype_new_i32());
  own wasm_func_t* fail_func =
    wasm_func_new_with_env(store, fail_type, fail_callback, store, NULL);

  wasm_functype_delete(fail_type);

  // Instantiate.
  printf("Instantiating module...\n");
  const wasm_extern_t* imports[] = { wasm_func_as_extern(fail_func) };
  own wasm_instance_t* instance =
    wasm_instance_new(store, module, imports, NULL);
  if (!instance) {
    printf("> Error instantiating module!\n");
    return 1;
  }

  wasm_func_delete(fail_func);

  // Extract export.
  printf("Extracting exports...\n");
  own wasm_extern_vec_t exports;
  wasm_instance_exports(instance, &exports);
  if (exports.size < 2) {
    printf("> Error accessing exports!\n");
    return 1;
  }

  wasm_module_delete(module);
  wasm_instance_delete(instance);

  // Call.
  for (int i = 0; i < 2; ++i) {
    char buf[32];
    const wasm_func_t* func = wasm_extern_as_func(exports.data[i]);
    if (func == NULL) {
      printf("> Error accessing export!\n");
      return 1;
    }

    printf("Calling export %d...\n", i);
    wasm_val_t results[1]; \
    own wasm_trap_t* trap = wasm_func_call(func, NULL, results);
    if (!trap) {
      printf("> Error calling function, expected trap!\n");
      return 1;
    }

    printf("Printing message...\n");
    own wasm_name_t message;
    wasm_trap_message(trap, &message);
    snprintf(buf, sizeof(buf), "> %%.%us\n", (unsigned)message.size);
    printf(buf, message.data);

    wasm_trap_delete(trap);
    wasm_name_delete(&message);
  }

  wasm_extern_vec_delete(&exports);

  // Shut down.
  printf("Shutting down...\n");
  wasm_store_delete(store);
  wasm_engine_delete(engine);

  // All done.
  printf("Done.\n");
  return 0;
}
