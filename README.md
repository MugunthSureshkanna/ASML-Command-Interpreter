# ASML-Command-Interpreter

A complete command interpreter and virtual machine implementation for ASML (A Simple Machine Language), built from the ground up in C. This project features a full-stack interpreter with lexical analysis, parsing, and execution of assembly-like programs.

Features
Complete Language Implementation: 24 core operations including arithmetic, bitwise, memory, and control flow instructions

Virtual Machine Architecture: 32 hardware registers, 64KB memory space, and stack-based function calls

Multi-stage Processing: Lexical analysis, recursive descent parsing, and interpreter execution

Memory Management: Manual memory allocation/deallocation with zero memory leaks

Comprehensive Testing: 150+ test cases covering all language features and edge cases

Architecture
text
Source Code → Lexer → Parser → Interpreter → Output
    ↓           ↓         ↓         ↓
  ASML     Tokens    AST/IR    Execution
Core Components
Lexer: Tokenizes input source code into meaningful units

Parser: Builds abstract syntax tree using recursive descent parsing

Interpreter: Executes parsed commands with register and memory management

Label Map: Custom hash table for efficient label resolution

Memory Subsystem: 64KB address space with load/store operations

Supported Operations
Category	Commands
Arithmetic	add, sub, mov
Comparison	cmp, cmp_u
Bitwise	and, eor, orr
Shifts	lsl, lsr, asr
Memory	load, store, put
Control Flow	b, b.cond, call, ret
I/O	print (decimal, hex, binary, string)
Building and Running
bash
# Build the project
make all

# Run the interpreter
./bin/ci -i input_file.asml -o output_file.txt

# Or run in interactive mode
./bin/ci
Example Programs
Basic Arithmetic
asml
mov x0 5
mov x1 10
add x2 x0 x1
print x2 d    # Output: 15
Hello World
asml
put "Hello World!" x0
print x0 s    # Output: Hello World!
Function Calls (Factorial)
asml
factorial:
    cmp x0 1
    b.le base_case
    mov x1 x0
    sub x0 x0 1
    call factorial
    mul x0 x0 x1
    ret
base_case:
    mov x0 1
    ret
Technical Implementation
Language: C (ISO C11 standard)

Memory: Manual management with malloc/free, validated with Valgrind

Data Structures: Custom hash table, linked lists, stack frames

Algorithms: Recursive descent parsing, depth-first command execution

Testing: Comprehensive test suite with 150+ test cases

Project Structure
text
asml-command-interpreter/
├── include/ci/          # Header files
│   ├── lexer.h         # Lexical analysis
│   ├── parser.h        # Syntax parsing  
│   ├── interpreter.h   # Command execution
│   └── command.h       # Command definitions
├── src/ci/             # Source implementation
│   ├── lexer.c         # Tokenization
│   ├── parser.c        # AST construction
│   ├── interpreter.c   # Virtual machine
│   └── label_map.c     # Hash table implementation
├── testcases/          # 150+ test programs
│   ├── week2/          # Basic operations
│   ├── week3/          # Memory & bitwise
│   └── week4/          # Control flow
└── Makefile            # Build configuration
Testing
The project includes extensive testing across all features:

bash
# Run test suites
make test_week2  # Basic arithmetic and I/O
make test_week3  # Memory operations and bitwise
make test_week4  # Control flow and functions
Key Achievements
Engineered complete assembly interpreter with 32 hardware registers

Implemented recursive descent parser and custom hash table for label resolution

Developed virtual memory subsystem handling 64KB address space

Achieved zero memory leaks through manual memory management

Built comprehensive test suite with 150+ test cases
