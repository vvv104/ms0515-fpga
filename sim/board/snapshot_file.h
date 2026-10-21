// snapshot_file: a snapshot's file, binary.  Verilator's own VerilatedSave
// and VerilatedRestore open the file with open(2) and no O_BINARY, which on
// Windows is a text file - a 0A written as 0D 0A, a 1A read as its end -
// and a model's state does not survive it.  These serialize through the
// same Verilator streams into an fstream opened binary.
#pragma once

#include <verilated_save.h>

#include <cstring>
#include <fstream>
#include <string>

class SnapshotSave final : public VerilatedSerialize {
public:
    explicit SnapshotSave(const std::string& path) : f_(path, std::ios::binary | std::ios::trunc)
    {
        m_filename = path;
        m_isOpen = f_.is_open();
        header();
    }
    ~SnapshotSave() override { close(); }

    void close() override
    {
        if (!m_isOpen) return;
        trailer();
        flush();
        f_.close();
        m_isOpen = false;
    }
    void flush() override
    {
        f_.write(reinterpret_cast<const char*>(m_bufp), static_cast<std::streamsize>(m_cp - m_bufp));
        m_cp = m_bufp;
    }

private:
    std::ofstream f_;
};

class SnapshotRestore final : public VerilatedDeserialize {
public:
    explicit SnapshotRestore(const std::string& path) : f_(path, std::ios::binary)
    {
        m_filename = path;
        m_isOpen = f_.is_open();
        m_cp = m_bufp;
        m_endp = m_bufp;
        header();
    }
    ~SnapshotRestore() override { close(); }

    void close() override
    {
        if (!m_isOpen) return;
        trailer();
        f_.close();
        m_isOpen = false;
    }

private:
    // the bytes not yet read to the buffer's start, then as many more as fit
    void fill() override
    {
        const std::size_t left = static_cast<std::size_t>(m_endp - m_cp);
        std::memmove(m_bufp, m_cp, left);
        m_cp = m_bufp;
        m_endp = m_bufp + left;
        f_.read(reinterpret_cast<char*>(m_endp), static_cast<std::streamsize>(bufferSize() - left));
        m_endp += f_.gcount();
    }

    std::ifstream f_;
};
