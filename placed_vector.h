/*
A chunked_vector is a mix between an array and a vector based on std::vector :
- like a vector its size and capacity may vary depending on how many elements you push into
- like an array it might be placed on the stack (with conditions)
A chunked_vector is indeed a vector (inherits from) with a custom allocator that will try to 
allocate on dedicated memory if possible. Like an array you give the capacity you expect your
container to have : under this capacity everything will be "in place" (= one the stack if
your chunked_vector is allocated on the stack), but once it overpass given capacity, provided
allocator will be used to allocate vector's memory.

Use cases :
- locality is important and you want to keep things on stack as much as possible
- number of elements container must be able to contain is uncertain

In the cheat sheet of containers :
- Order is important : yes and no
- Nor LIFO, nor FIFO, nor LEFO
- Sorted by key : no
- Find by key : no
- Insert/erase in front : no
- Need to find the nth element : yes
- Insert/erase in middle : no
- Merge collections : no
- Size will vary widely : no

NOTE : provided Allocator will only be used if initial N capacity is exceeded.
*/

#pragma once
#include <vector>

namespace vob
{
    template <typename T>
    struct place_manager
    {
        typedef char byte;
        bool is_used;
        bool is_enabled;
        byte* p_bytes;
        std::size_t size;

        place_manager(byte* p_bytes, std::size_t size)
            : is_used(false)
            , is_enabled(true)
            , p_bytes(p_bytes)
            , size(size)
        {}
    };

    template <typename T, std::size_t N>
    struct managed_place : public place_manager<T>
    {
        byte bytes[N * sizeof(T) / sizeof(byte)];

        managed_place()
            : place_manager(&bytes[0], N)
        {}
    };

    template <typename T, typename allocator_type = std::allocator<T>>
    class allocator_in_place
    {
    public:
        typedef T value_type;
        typedef T* pointer;
        typedef const T* const_pointer;
        typedef T& reference;
        typedef const T& const_reference;
        typedef std::size_t size_type;
        typedef std::ptrdiff_t difference_type;

        allocator_in_place(const allocator_type& alloc = allocator_type(), place_manager<T>* place = nullptr)
            : m_allocator(alloc)
            , m_place(place)
        {}
        allocator_in_place(const allocator_in_place<T, allocator_type>& alloc)
            : m_allocator(alloc.m_allocator)
            , m_place(alloc.m_place)
        {}
        template <typename U>
        allocator_in_place(const allocator_in_place<U>& other) noexcept
            : m_allocator()
            , m_place(nullptr)
        {}

        template <typename U>
        struct rebind
        {
            typedef allocator_in_place<U> other;
        };

        pointer address(reference x) const
        {
            return &x;
        }
        const_pointer address(const_reference x) const
        {
            return &x;
        }
        pointer allocate(size_type n, const_pointer hint = 0)
        {
            if (m_place && m_place->is_enabled && !m_place->is_used && n <= m_place->size)
            {
                m_place->is_used = true;
                return (pointer)m_place->p_bytes;
            }
            return m_allocator.allocate(n, hint);
        }
        void deallocate(pointer p, size_type n)
        {
            if (p == (pointer)m_place->p_bytes)
            {
                m_place->is_used = false;
                return;
            }
            m_allocator.deallocate(p, n);
        }
        size_type max_size() const throw()
        {
            return m_allocator.max_size();
        }
        void construct(pointer p, const_reference val)
        {
            ::new((void*)p) value_type(val);
        }
        void destroy(pointer p)
        {
            p->~value_type();
        }

        bool is_in_place() const
        {
            if (!m_place)
            {
                return false;
            }
            return m_place->is_used;
        }

        allocator_type get_allocator() const
        {
            return m_allocator;
        }

    private:
        allocator_type m_allocator;
        place_manager<T>* m_place;
    };

    template <typename T, std::size_t N = 8, typename A = std::allocator<T>>
    class placed_vector : public std::vector<T, allocator_in_place<T, A>>
    {
    public:
        typedef allocator_in_place<T, A> _allocator_type;

        // Constructors & destructor
        placed_vector(const A& alloc = A())
            : std::vector<T, _allocator_type>(_allocator_type(alloc, &m_place))
        {
            reserve(N);
        }

        placed_vector(const placed_vector& other)
            : std::vector<T, _allocator_type>(other, _allocator_type(get_allocator().get_allocator(), &m_place))
        {}

        std::size_t size_in_place()
        {
            return N;
        }

        bool is_in_place() const
        {
            return get_allocator().is_in_place();
        }

        bool put_in_place()
        {
            if (is_in_place() && capacity() == N)
            {
                return true;
            }
            if (size() > N)
            {
                return false;
            }
            reserve(N + 1);
            if (size() != N)
            {
                m_place.is_enabled = false;
            }
            shrink_to_fit();
            m_place.is_enabled = true;
            reserve(N);
            if (is_in_place())
            {
                return true;
            }
            return false;
        }

    private:
        managed_place<T, N> m_place;
    };
}
