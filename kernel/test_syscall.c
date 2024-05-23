char *name = "shared_memory";
int descriptor;

// Create an existing object
shm_objects[0].name = "shared_memory";
shm_objects[0].ref_count = 1;

// Call the sys_shm_open function
descriptor = sys_shm_open(name);

// Check if the descriptor is correct
// The expected result is 0 since the object already exists
assert(descriptor == 0);