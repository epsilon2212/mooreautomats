#include <stddef.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#define BITS_TO_WORDS(x) ((x+63)/64)

typedef void (*transition_function_t)(uint64_t *next_state, uint64_t const *input, uint64_t const *state, size_t n, size_t s);

typedef void (*output_function_t)(uint64_t *output, uint64_t const *state, size_t m, size_t s);

typedef struct moore moore_t;
typedef struct outgoing outgoing_t;
typedef struct incoming incoming_t;

typedef struct outgoing {
    moore_t * aut;
    size_t bit;
    outgoing_t * next;
} outgoing_t;

typedef struct incoming {
    moore_t * aut;
    size_t bit; //numer bitu ktorego dotyczny *aut
} incoming_t;

typedef struct moore {
    size_t inputSize, stateSize, outputSize; // w bitach
    uint64_t * inputArray;   // tablica z wejściem rozmiaru (inputSize + 63)/64;
    uint64_t * stateArray;   // tablica ze stanem rozmiaru (stateSize + 63)/64;
    uint64_t * outputArray;  // tablica z wyjsciem rozmiaru (outputSize + 63)/64;
    transition_function_t t_func; // funkcja przejścia
    output_function_t y_func;     // funkcja wyjścia
    incoming_t ** incoming; //tablica wskaznikow na automaty od ktorych przyjmujemy wejscie [rozmiaru inputSize]
    outgoing_t ** outgoing; //tablica list ze wskaznikami na automaty przyjmujace bity od tego automatu [rozmiaru outputSize]
} moore_t;

static bool getBit(uint64_t word, size_t shift)
{
    //zwaraca 1 jesli bit na pozycji shift w slowie word jest 1, 0 wpp
    const uint64_t mask = 1ULL << shift;
    return (word & mask) != 0;
}

static void setBit(uint64_t * array, size_t shift, bool value)
{
    //ustawia bit o numerze shift licząc od najmłodszego bitu w tablicy array na value.
    size_t word = shift/64;
    shift = shift % 64;
    if (value)
    {
        array[word] |= (1ULL << shift);
    }
    else
    {
        array[word] &= ~(1ULL << shift);
    }
}

static void clearBits(uint64_t * array, size_t index, size_t shift)
{
    //czyści nieuzywane bity. shift to taka liczba, ze bit do pozycji shift pozostaje nie zmieniony (licząc od najmlodszego bitu). index to numer slowa w tablicy array
    //ktore chcemy modyfikować (de facto ostatni element)
    const uint64_t mask = (1ULL << shift) - 1;
    array[index] &= mask;
}
static void identity_output(uint64_t *output, const uint64_t *state, size_t m, size_t s)
{
    m = s;
    const size_t words = BITS_TO_WORDS(m);
    memcpy(output, state, words * sizeof(uint64_t));
}

moore_t * ma_create_full(size_t n, size_t m, size_t s, transition_function_t t, output_function_t y, uint64_t const *q)
{
    if (m == 0 || s == 0 || !t || !y || !q)
    {
        errno = EINVAL;
        return NULL;
    }
    moore_t * automata = malloc(sizeof(moore_t));
    if (!automata)
    {
        errno = ENOMEM;
        return NULL;
    }

    automata->inputSize = n;
    automata->stateSize = s;
    automata->outputSize = m;
    automata->t_func = t;
    automata->y_func = y;

    const size_t inputWords = BITS_TO_WORDS(n);
    const size_t stateWords = BITS_TO_WORDS(s);
    const size_t outputWords = BITS_TO_WORDS(m);

    bool SKIP_INPUT_ARRAY = (n == 0);

    automata->inputArray = (!SKIP_INPUT_ARRAY) ? calloc(inputWords, sizeof(uint64_t)) : NULL;
    automata->stateArray = calloc(stateWords, sizeof(uint64_t));
    automata->outputArray = calloc(outputWords, sizeof(uint64_t));
    automata->incoming = (!SKIP_INPUT_ARRAY) ? calloc(n, sizeof(incoming_t*)) : NULL;
    automata->outgoing = calloc(m, sizeof(outgoing_t*));

    if ((!SKIP_INPUT_ARRAY && !automata->inputArray) || (!SKIP_INPUT_ARRAY && !automata->incoming) || !automata->stateArray ||
        !automata->outputArray || !automata->outgoing)
    {
        free(automata->incoming);
        free(automata->inputArray);
        free(automata->outgoing);
        free(automata->outputArray);
        free(automata->stateArray);
        free(automata);
        errno = ENOMEM;
        return NULL;
    }

    if (!SKIP_INPUT_ARRAY)
    {
        for (size_t i = 0; i < n; i++)
        {
            automata->incoming[i] = NULL;
        }
    }
    for (size_t i = 0; i < m; i++)
    {
        automata->outgoing[i] = NULL;
    }

    memcpy(automata->stateArray, q, stateWords * sizeof(uint64_t));
    if (s % 64 != 0)
    {
        clearBits(automata->stateArray, stateWords - 1, s % 64);
    }
   
    y(automata->outputArray, automata->stateArray, m, s);
    if (m % 64 != 0)
    {
        clearBits(automata->outputArray, outputWords - 1, m % 64);
    }

    return automata;
}

//tworzy uproszczony automat za pomocą funkcji ma_create_full
moore_t * ma_create_simple(size_t n, size_t m, transition_function_t t)
{
    if (m == 0 || !t)
    {
        errno = EINVAL;
        return NULL;
    }

    const size_t stateWords = BITS_TO_WORDS(m);
    uint64_t * initialState = calloc(stateWords, sizeof(uint64_t)); //calloc uzuepłnia tablice zerami
    if (!initialState)
    {
        errno = ENOMEM;
        return NULL;
    }

    moore_t *automata = ma_create_full(n, m, m, t, identity_output, initialState);
    free(initialState);
    return automata;
}


int ma_connect(moore_t *a_in, size_t in, moore_t *a_out, size_t out, size_t num);
int ma_disconnect(moore_t *a_in, size_t in, size_t num);

void ma_delete(moore_t *a)
{
    if (!a) return;
    //najpierw odlaczamy przychodzące połączenia
    if (a->inputSize > 0) ma_disconnect(a, 0, a->inputSize);

    //dla kazdego automatu ktory otrzymuje i-ty bit z wyjscia automatu a, 
    for (size_t i = 0; i < a->outputSize; i++)
    {
        outgoing_t * current = a->outgoing[i];
        while (current)
        {
            outgoing_t * next = current->next;
            
            if (current->aut && current->bit < current->aut->inputSize)
            {
                incoming_t * temp = current->aut->incoming[current->bit];
                if (temp && temp->aut == a)
                {
                    free(temp);
                    current->aut->incoming[current->bit] = NULL;
                }
            }
            
            free(current);
            current = next;
        }
    }
    for (size_t i = 0; i < a->inputSize; i++) free(a->incoming[i]);
    free(a->incoming);
    free(a->outgoing);
    free(a->inputArray);
    free(a->stateArray);
    free(a->outputArray);
    free(a);
}

int ma_connect(moore_t *a_in, size_t in, moore_t *a_out, size_t out, size_t num)
{
    if (!a_in || !a_out || num == 0 || in + num > a_in->inputSize || out + num > a_out->outputSize)
    {
        errno = EINVAL;
        return -1;
    }
    for (size_t i = 0; i < num; i++)
    {
        size_t inputIndex = i + in;
        size_t outputIndex = i + out;

        //jezeli polaczenie juz istnieje, to najpierw musimy je usunąć
        if (a_in->incoming[inputIndex] != NULL)
        {
            incoming_t * oldIncoming = a_in->incoming[inputIndex];
            outgoing_t * prevOut = NULL;
            outgoing_t * currOut = oldIncoming->aut->outgoing[oldIncoming->bit];

            while (currOut)
            {
                if (currOut->aut == a_in && currOut->bit == inputIndex)
                {
                    if (prevOut)
                    {
                        prevOut->next = currOut->next;
                    }
                    else
                    {
                        oldIncoming->aut->outgoing[oldIncoming->bit] = currOut->next;
                    }
                    free(currOut);
                    break;
                }
                prevOut = currOut;
                currOut = currOut->next;
            }
        }
        else
        {
            incoming_t * temp = malloc(sizeof(incoming_t));
            if (!temp) {
                errno = ENOMEM;
                return -1;
            }
            a_in->incoming[inputIndex] = temp;
        }
        a_in->incoming[inputIndex]->aut = a_out;
        a_in->incoming[inputIndex]->bit = outputIndex;
        
        // sprawdzamy czy istnieje już wpis dla tego automatu i bitu
        bool wasThereAlready = false;
        outgoing_t * traverse = a_out->outgoing[outputIndex];
        outgoing_t * prev = NULL;

        while (traverse != NULL && !wasThereAlready)
        {
            if (traverse->aut == a_in && traverse->bit == inputIndex)
            {
                wasThereAlready = true;
            }
            prev = traverse;
            traverse = traverse->next;
        }

        //wstawiamy element listy ktory bedzie wskazywal na zadany automat
        if (!wasThereAlready)
        {
            outgoing_t * newNode = malloc(sizeof(outgoing_t));
            if (!newNode) {
                errno = ENOMEM;
                return -1;
            }
            newNode->aut = a_in;
            newNode->bit = inputIndex;
            newNode->next = NULL;

            if (prev)
            {
                prev->next = newNode;
            } else {
                a_out->outgoing[outputIndex] = newNode;
            }
        }
    }
    return 0;
}

int ma_disconnect(moore_t *a_in, size_t in, size_t num)
{
    if (!a_in || num == 0 || in + num > a_in->inputSize) {
        errno = EINVAL;
        return -1;
    }

    for (size_t i = 0; i < num; i++)
    {
        size_t inputIndex = i + in;

        incoming_t *temp = a_in->incoming[inputIndex];
        if (!temp) continue;

        moore_t *source = temp->aut;
        size_t source_bit = temp->bit;

        a_in->incoming[inputIndex] = NULL;

        //usuwamy polaczenia ktore wychodzily z a_in
        if (source && source_bit < source->outputSize)
        {
            outgoing_t *ptr = source->outgoing[source_bit];
            outgoing_t *prev = NULL;

            while (ptr)
            {
                if (ptr->aut == a_in && ptr->bit == inputIndex)
                {
                    if (prev)
                    {
                        prev->next = ptr->next;
                    } else {
                        source->outgoing[source_bit] = ptr->next;
                    }
                    free(ptr);
                    break;
                }
                prev = ptr;
                ptr = ptr->next;
            }
        }

        free(temp);
        a_in->incoming[inputIndex] = NULL;
    }
    return 0;
}

//zgodnie z definicja ignoruje bity podlaczone
int ma_set_input(moore_t *a, uint64_t const *input)
{
    if (!a || !input || a->inputSize == 0)
    {
        errno = EINVAL;
        return -1;
    }
    for (size_t i = 0; i < a->inputSize; i++)
    {
        if (a->incoming[i] == NULL)
        {
            size_t index = i / 64;
            size_t shift = i % 64;

            bool sign = getBit(input[index], shift);
            setBit(a->inputArray, i, sign);
        }
    }
    return 0;
}

int ma_set_state(moore_t *a, uint64_t const *state)
{
    if (!a || !state)
    {
        errno = EINVAL;
        return -1;
    }
    size_t wordsState = BITS_TO_WORDS(a->stateSize);
    memcpy(a->stateArray, state, wordsState * sizeof(uint64_t));
    if (a->stateSize % 64 != 0) clearBits(a->stateArray, wordsState - 1, a->stateSize % 64);
    
    a->y_func(a->outputArray, a->stateArray, a->outputSize, a->stateSize);
    return 0;
}

uint64_t const * ma_get_output(moore_t const *a)
{
    if (!a)
    {
        errno = EINVAL;
        return NULL;
    }
    return a->outputArray;
}

int ma_step(moore_t *at[], size_t num)
{
    if (!at || num == 0)
    {
        errno = EINVAL;
        return -1;
    }
    for (size_t i = 0; i < num; i++)
    {
        if (at[i] == NULL)
        {
            errno = EINVAL;
            return -1;
        }
    }

    //aktualizujemy wejscie pobierajac bity jesli jest polaczenie
    for (size_t i = 0; i < num; i++)
    {
        moore_t * current = at[i];
        for (size_t j = 0; j < current->inputSize; j++)
        {
            incoming_t * temp = current->incoming[j];
            if (temp != NULL)
            {
                bool sign = getBit(temp->aut->outputArray[temp->bit/64], temp->bit % 64);
                setBit(current->inputArray, j, sign);
            }
        }
    }
    //aktualizujemy tablice stanu i wyjscia jednoczesnie, przy okazji czysczac niezuywane bity
    for (size_t i = 0; i < num; i++)
    {
        moore_t * current = at[i];

        size_t wordsState = BITS_TO_WORDS(current->stateSize);
        size_t wordsOutput = BITS_TO_WORDS(current->outputSize);
        uint64_t * newState = calloc(wordsState, sizeof(uint64_t));
        if (!newState)
        {
            errno = ENOMEM;
            return -1;
        }

        current->t_func(newState, current->inputArray, current->stateArray, current->inputSize, current->stateSize);
        memcpy(current->stateArray, newState, wordsState * sizeof(uint64_t));
        
        if (current->stateSize % 64 != 0) clearBits(current->stateArray, wordsState - 1, current->stateSize % 64);

        current->y_func(current->outputArray, current->stateArray, current->outputSize, current->stateSize);

        if (current->outputSize % 64 != 0) clearBits(current->outputArray, wordsOutput - 1, current->outputSize % 64);
        free(newState);
    }
    return 0;
}