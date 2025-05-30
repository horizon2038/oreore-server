#ifndef OREORE_SCOPED_FILE_DESCRIPTOR_HPP
#define OREORE_SCOPED_FILE_DESCRIPTOR_HPP

#include <unistd.h>

namespace oreore
{
    class scoped_file_descriptor
    {
      private:
        int fd;

      public:
        explicit scoped_file_descriptor(int target_fd = -1) : fd(target_fd)
        {
        }

        ~scoped_file_descriptor(void)
        {
            if (fd != -1)
            {
                ::close(fd);
            }
        }

        scoped_file_descriptor(const scoped_file_descriptor &) = delete;
        auto operator=(const scoped_file_descriptor &)
            -> scoped_file_descriptor & = delete;

        scoped_file_descriptor(scoped_file_descriptor &&other) noexcept
            : fd(other.fd)
        {
            other.fd = -1;
        }

        auto operator=(scoped_file_descriptor &&other) noexcept
            -> scoped_file_descriptor &
        {
            if (this != &other)
            {
                if (fd != -1)
                {
                    ::close(fd);
                }
                fd       = other.fd;
                other.fd = -1;
            }

            return *this;
        }

        auto get(void) const -> int
        {
            return fd;
        }

        auto release(void) -> int
        {
            int temp_fd = fd;
            fd          = -1;

            return temp_fd;
        }
    };
}

#endif
