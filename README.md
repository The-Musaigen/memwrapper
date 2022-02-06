# memwrapper
A wrapper for memory manipulation. Includes: low-level memory operations (read, write, copy, fill, compare), signature search, hooks and patches.

## Usage

Clone repository to your project and include `memwrapper.h`. `C++17` compiler compatible required.

## Examples: Low-level memory operations (LLMO)
```cpp
int main()
{
    // all llmo examples will be shown on this variable
    int a = 5;
    
    // writing
    memwrapper::write_memory<int>(&a, 10); // will set variable 'a' value to 10
    
    // reading
    int result = memwrapper::read_memory<int>(&a); // output: 10
    
    // retn instruction, just for test
    // will set variable 'a' value to 195
    memwrapper::copy_memory(&a, "\xC3", 1); 
    
    // filling
    memwrapper::fill_memory(&a, 144, 1); // will set variable 'a' value to 144
    
    // compare
    int diff = memwrapper::compare_memory(&a, "\x90", 1); // output: 0, so a == 144
}
```
## Examples: RAII LLMO
```cpp
int main()
{
    // all raii llmo examples will be shown on this variable
    int a = 5;
    {
        memwrapper::scoped_write<int> raii{ &a, 15 };

        std::cout << a << std::endl; // output: 15
    }
    // now variable 'a' is 5
       
    // memwrapper::scoped_copy<size> and memwrapper::scoped_fill<size> is the same, but...
    // you need to specify the size of instructions that will be patched.
    // example:
    /*
        // asm listing in some program (win86)
        0x435AB0: jmp 0x11223344
    */
    memwrapper::scoped_copy<5> nop_jmp{ 0x435AB0, "\x90\x90\x90\x90\x90" };
}
```
## Examples: Signature search
```cpp
int main()
{
    // example pattern: 
    // 'x' - required byte
    // '?' - optional byte 
    uintptr_t address = memwrapper::search_memory_pattern("application.exe", "\xEB\x24\xE9\x00\x00\x00\x00", "xxx????");
    // will be return the address of the specified signature
    // or 0 if signature isn't found.
}
```
## Examples: Hooking
```cpp
// Arguments is optional in current implementation.
using sum_t = int(__cdecl*)(int, int);

// Also hooks doesn't passing into args of 'sum-hooked' in current implementation.
std::unique_ptr<memwrapper::memhook<sum_t>> hook_sum;

int _declspec(noinline) sum(int a, int b)
{
    return (a + b);
}

int _declspec(noinline) sum_hooked(int a, int b)
{
    std::cout << a << " " << b << std::endl; // output: 1, 2
    return hook_sum->call(a + 4, b);
}

int main()
{
    hook_sum = std::make_unique<memwrapper::memhook<sum_t>>(sum, sum_hooked);
    hook_sum->install();

    std::cout << sum(1, 2) << std::endl; // output: 7
    // hook_sum's destructor will be automatically called.
}
```
## Examples: Patching
```cpp
int main()
{
    memwrapper::scoped_patch patch;
    // first argument - address
    // second argument - vector, bytes that will be written
    // third argument (optional) - vector, original bytes
    patch.add(memwrapper::scoped_patch_unit{ 0x11223344, {0xEB, 0x74}, {0x90, 0xC3} /*optional*/ });
    // same as above, but first argument (optional) - module that will be patched
    patch.add(memwrapper::scoped_patch_unit{ "module.dll", 0x11223344, {0xEB, 0x74}, {0x90, 0xC3} /*optional*/ });

    /* or */
    memwrapper::scoped_patch_unit unit{ 0x11223344, {0xEB, 0x74} };
    patch.add(unit);

    /* or */
    memwrapper::scoped_patch patch1
    {
        {
            {"module.dll", 0x654321, std::vector<uint8_t>(6, 0x90), {0x8B, 0x0D, 0x11, 0x22, 0x33, 0x44}}
        }
    };

    // install/remove/toggle
    patch.install();
    patch.remove();
    patch.toggle(true);

    // patch's destructor will be automatically called
    // and bytes will be restored
}
```
# Credits
Hacker Disassembler Engine (HDE) - Vyacheslav Patkov

# License
memwrapper is licensed under the MIT License, see 'LICENSE' for more information.
