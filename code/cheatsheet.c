/*
 * fetch uuid_t as key and stores the value into memory address
 */
unqlite_kv_fetch(db, address of uuid_t, keysize, memory address, size of value of address);


/*
 * store object at memory address with uuid_t as key
 */
unqlite_kv_store(db, address of uuid_t, keysize, memory address, size of value of address);


//generates new random key and puts at uuid_t
uuid_generate(uuid_t)

