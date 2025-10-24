Moore Automata
The task is to implement, in the C language, a dynamically loaded library that simulates Moore automata. A Moore automaton is a type of deterministic finite-state machine used in synchronous digital systems. It is represented as an ordered six-tuple ⟨𝑋, 𝑌, 𝑄, 𝑡, 𝑦, 𝑞⟩, where

* 𝑋 is the set of values taken by the input signals,
* 𝑌 is the set of values taken by the output signals,
* 𝑄 is the set of internal state values,
* 𝑡: 𝑋×𝑄→𝑄 is the transition function,
* 𝑦: 𝑄→𝑌 is the output function,
* 𝑞∈𝑄 is the initial state.

We consider only binary automata, which have 𝑛 single-bit input signals, 𝑚 single-bit output signals, and a state consisting of 𝑠 bits. Formally, 𝑋={0,1}ⁿ, 𝑌={0,1}ᵐ, 𝑄={0,1}ˢ.

At each step of operation, the function 𝑡 computes the new state of the automaton based on the current input signal values and the current state. The function 𝑦 computes the output signal values based on the automaton’s state.

### Library Interface

The interface of the library is provided in the header file `ma.h` attached to the task description. Additional implementation details can be inferred from the accompanying example file `ma_example.c`, which is an integral part of the specification.

Bit sequences and signal values are stored in an array of elements of type `uint64_t`. Each element stores 64 consecutive bits, starting from the least significant bit. If the sequence length is not a multiple of 64, the higher bits of the last element remain unused.

The structural type `moore_t` represents an automaton and must be defined (implemented) as part of this task:

```c
typedef struct moore moore_t;
```

The automaton’s state and the input/output signals are represented as bit sequences.

The type `transition_function_t` represents the automaton’s transition function. It computes the new state based on the input signals and the current state.

```c
typedef void (*transition_function_t)(uint64_t *next_state, uint64_t const *input, uint64_t const *state, size_t n, size_t s);
```

Parameters:

* `next_state` – pointer to the bit sequence storing the new state,
* `input` – pointer to the bit sequence with input signal values,
* `state` – pointer to the bit sequence with the current state,
* `n` – number of input signals,
* `s` – number of bits in the internal state.

The type `output_function_t` represents the output function, which computes the automaton’s output signals based on its state:

```c
typedef void (*output_function_t)(uint64_t *output, uint64_t const *state, size_t m, size_t s);
```

Parameters:

* `output` – pointer to the bit sequence where output signal values are stored,
* `state` – pointer to the bit sequence containing the current state,
* `m` – number of output signals,
* `s` – number of bits in the internal state.

---

### Function Specifications

**`moore_t * ma_create_full(size_t n, size_t m, size_t s, transition_function_t t, output_function_t y, uint64_t const *q);`**
Creates a new automaton.

Parameters:

* `n` – number of input signals,
* `m` – number of output signals,
* `s` – number of bits in the internal state,
* `t` – transition function,
* `y` – output function,
* `q` – pointer to the bit sequence representing the initial state.

Returns:

* pointer to the created automaton structure, or
* `NULL` if `m` or `s` equals zero, or any of `t`, `y`, `q` is `NULL`, or memory allocation fails (sets `errno` to `EINVAL` or `ENOMEM` respectively).

---

**`moore_t * ma_create_simple(size_t n, size_t s, transition_function_t t);`**
Creates a new simple automaton, where the number of outputs equals the number of state bits, the output function is the identity, and the initial state is zeroed. Unused state bits are initialized to zero.

Returns:

* pointer to the created automaton, or
* `NULL` if `s` equals zero, `t` is `NULL`, or memory allocation fails (sets `errno` accordingly).

---

**`void ma_delete(moore_t *a);`**
Deletes the given automaton and frees all associated memory. Does nothing if called with `NULL`. After this call, the pointer becomes invalid.

---

**`int ma_connect(moore_t *a_in, size_t in, moore_t *a_out, size_t out, size_t num);`**
Connects `num` consecutive input signals of automaton `a_in` (starting from `in`) to the output signals of automaton `a_out` (starting from `out`). Disconnects any previous connections for those inputs if necessary.

Returns:

* `0` on success,
* `-1` if any pointer is `NULL`, `num` is zero, the signal ranges are invalid, or memory allocation fails (sets `errno` to `EINVAL` or `ENOMEM`).

---

**`int ma_disconnect(moore_t *a_in, size_t in, size_t num);`**
Disconnects `num` consecutive input signals of automaton `a_in` starting from `in`. Inputs that were not connected remain unconnected.

Returns:

* `0` on success,
* `-1` if `a_in` is `NULL`, `num` is zero, or the input range is invalid (sets `errno` to `EINVAL`).

---

**`int ma_set_input(moore_t *a, uint64_t const *input);`**
Sets the values of signals on the unconnected inputs of the automaton, ignoring bits corresponding to connected inputs.

Returns:

* `0` on success,
* `-1` if the automaton has no inputs or if any pointer is `NULL` (sets `errno` to `EINVAL`).

---

**`int ma_set_state(moore_t *a, uint64_t const *state);`**
Sets the internal state of the automaton.

Returns:

* `0` on success,
* `-1` if any pointer is `NULL` (sets `errno` to `EINVAL`).

---

**`uint64_t const * ma_get_output(moore_t const *a);`**
Returns a pointer to the bit sequence representing the automaton’s output signal values. The pointer remains valid until `ma_delete` is called on the automaton.

Returns:

* pointer to the output bit sequence, or
* `NULL` if `a` is `NULL` (sets `errno` to `EINVAL`).

---

**`int ma_step(moore_t *at[], size_t num);`**
Performs one simulation step for the given automata. All automata operate synchronously and in parallel, meaning that new states and outputs after the call depend only on the states, inputs, and outputs before the call.

Returns:

* `0` on success,
* `-1` if the array pointer is `NULL`, any element of it is `NULL`, `num` equals zero, or memory allocation fails (sets `errno` accordingly).

---

### Functional Requirements

* Input signals are set either using `ma_set_input` or by connecting an input to another automaton’s output.
* An input remains **undefined** until it has been assigned or connected. After disconnecting, the input becomes undefined again. An undefined signal state means its value is unknown.
* When deleting or connecting automata, care must be taken to properly disconnect them to avoid dangling connections.
* The library must provide **weak failure resistance** to memory allocation errors: it must not leak memory, and all data structures must remain in a consistent state.
