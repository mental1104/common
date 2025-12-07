using System;
using System.IO;
using System.Runtime.InteropServices;

namespace Mental1104.Executables;

[StructLayout(LayoutKind.Sequential)]
internal struct IMAGE_DOS_HEADER
{
    public ushort e_magic;
    public ushort e_cblp;
    public ushort e_cp;
    public ushort e_crlc;
    public ushort e_cparhdr;
    public ushort e_minalloc;
    public ushort e_maxalloc;
    public ushort e_ss;
    public ushort e_sp;
    public ushort e_csum;
    public ushort e_ip;
    public ushort e_cs;
    public ushort e_lfarlc;
    public ushort e_ovno;
    public uint e_res1;
    public uint e_res2;
    public ushort e_oemid;
    public ushort e_oeminfo;
    public uint e_res3;
    public uint e_res4;
    public uint e_res5;
    public uint e_res6;
    public uint e_res7;
    public int e_lfanew;
}

[StructLayout(LayoutKind.Sequential)]
internal struct IMAGE_FILE_HEADER
{
    public ushort Machine;
    public ushort NumberOfSections;
    public uint TimeDateStamp;
    public uint PointerToSymbolTable;
    public uint NumberOfSymbols;
    public ushort SizeOfOptionalHeader;
    public ushort Characteristics;
}

[StructLayout(LayoutKind.Sequential)]
internal struct IMAGE_NT_HEADERS_COMMON
{
    public uint Signature;
    public IMAGE_FILE_HEADER FileHeader;
}

[StructLayout(LayoutKind.Sequential)]
internal struct IMAGE_NT_HEADERS32
{
    public uint Signature;
    public IMAGE_FILE_HEADER FileHeader;
    public IMAGE_OPTIONAL_HEADER32 OptionalHeader;
}

[StructLayout(LayoutKind.Sequential)]
internal struct IMAGE_NT_HEADERS64
{
    public uint Signature;
    public IMAGE_FILE_HEADER FileHeader;
    public IMAGE_OPTIONAL_HEADER64 OptionalHeader;
}

[StructLayout(LayoutKind.Sequential)]
internal struct IMAGE_OPTIONAL_HEADER32
{
    public ushort Magic;
    public byte MajorLinkerVersion;
    public byte MinorLinkerVersion;
    public uint SizeOfCode;
    public uint SizeOfInitializedData;
    public uint SizeOfUninitializedData;
    public uint AddressOfEntryPoint;
    public uint BaseOfCode;
    public uint BaseOfData;
    public uint ImageBase;
    public uint SectionAlignment;
    public uint FileAlignment;
    public ushort MajorOperatingSystemVersion;
    public ushort MinorOperatingSystemVersion;
    public ushort MajorImageVersion;
    public ushort MinorImageVersion;
    public ushort MajorSubsystemVersion;
    public ushort MinorSubsystemVersion;
    public uint Win32VersionValue;
    public uint SizeOfImage;
    public uint SizeOfHeaders;
    public uint CheckSum;
    public ushort Subsystem;
    public ushort DllCharacteristics;
    public uint SizeOfStackReserve;
    public uint SizeOfStackCommit;
    public uint SizeOfHeapReserve;
    public uint SizeOfHeapCommit;
    public uint LoaderFlags;
    public uint NumberOfRvaAndSizes;
}

[StructLayout(LayoutKind.Sequential)]
internal struct IMAGE_OPTIONAL_HEADER64
{
    public ushort Magic;
    public byte MajorLinkerVersion;
    public byte MinorLinkerVersion;
    public uint SizeOfCode;
    public uint SizeOfInitializedData;
    public uint SizeOfUninitializedData;
    public uint AddressOfEntryPoint;
    public uint BaseOfCode;
    public ulong ImageBase;
    public uint SectionAlignment;
    public uint FileAlignment;
    public ushort MajorOperatingSystemVersion;
    public ushort MinorOperatingSystemVersion;
    public ushort MajorImageVersion;
    public ushort MinorImageVersion;
    public ushort MajorSubsystemVersion;
    public ushort MinorSubsystemVersion;
    public uint Win32VersionValue;
    public uint SizeOfImage;
    public uint SizeOfHeaders;
    public uint CheckSum;
    public ushort Subsystem;
    public ushort DllCharacteristics;
    public ulong SizeOfStackReserve;
    public ulong SizeOfStackCommit;
    public ulong SizeOfHeapReserve;
    public ulong SizeOfHeapCommit;
    public uint LoaderFlags;
    public uint NumberOfRvaAndSizes;
}

public static class ExeChecker
{
    public static bool IsValidExe(string fileName)
    {
        if (!File.Exists(fileName))
            return false;

        try
        {
            using var stream = File.OpenRead(fileName);

            IMAGE_DOS_HEADER dosHeader = GetDosHeader(stream);
            if (dosHeader.e_magic != IMAGE_DOS_SIGNATURE)
                return false;

            IMAGE_NT_HEADERS_COMMON ntHeader = GetCommonNtHeader(stream, dosHeader);
            if (ntHeader.Signature != IMAGE_NT_SIGNATURE)
                return false;

            if ((ntHeader.FileHeader.Characteristics & IMAGE_FILE_DLL) != 0)
                return false;

            return ntHeader.FileHeader.Machine switch
            {
                IMAGE_FILE_MACHINE_I386 => IsValidExe32(GetNtHeader32(stream, dosHeader)),
                IMAGE_FILE_MACHINE_IA64 => IsValidExe64(GetNtHeader64(stream, dosHeader)),
                IMAGE_FILE_MACHINE_AMD64 => IsValidExe64(GetNtHeader64(stream, dosHeader)),
                _ => false
            };
        }
        catch (InvalidOperationException)
        {
            return false;
        }
    }

    private static bool IsValidExe32(IMAGE_NT_HEADERS32 ntHeader) =>
        ntHeader.OptionalHeader.Magic == IMAGE_NT_OPTIONAL_HDR32_MAGIC;

    private static bool IsValidExe64(IMAGE_NT_HEADERS64 ntHeader) =>
        ntHeader.OptionalHeader.Magic == IMAGE_NT_OPTIONAL_HDR64_MAGIC;

    private static IMAGE_DOS_HEADER GetDosHeader(Stream stream)
    {
        stream.Seek(0, SeekOrigin.Begin);
        return ReadStructFromStream<IMAGE_DOS_HEADER>(stream);
    }

    private static IMAGE_NT_HEADERS_COMMON GetCommonNtHeader(Stream stream, IMAGE_DOS_HEADER dosHeader)
    {
        stream.Seek(dosHeader.e_lfanew, SeekOrigin.Begin);
        return ReadStructFromStream<IMAGE_NT_HEADERS_COMMON>(stream);
    }

    private static IMAGE_NT_HEADERS32 GetNtHeader32(Stream stream, IMAGE_DOS_HEADER dosHeader)
    {
        stream.Seek(dosHeader.e_lfanew, SeekOrigin.Begin);
        return ReadStructFromStream<IMAGE_NT_HEADERS32>(stream);
    }

    private static IMAGE_NT_HEADERS64 GetNtHeader64(Stream stream, IMAGE_DOS_HEADER dosHeader)
    {
        stream.Seek(dosHeader.e_lfanew, SeekOrigin.Begin);
        return ReadStructFromStream<IMAGE_NT_HEADERS64>(stream);
    }

    private static T ReadStructFromStream<T>(Stream stream)
    {
        int structSize = Marshal.SizeOf(typeof(T));
        IntPtr memory = IntPtr.Zero;

        try
        {
            memory = Marshal.AllocCoTaskMem(structSize);
            if (memory == IntPtr.Zero)
                throw new InvalidOperationException();

            byte[] buffer = new byte[structSize];
            int bytesRead = stream.Read(buffer, 0, structSize);
            if (bytesRead != structSize)
                throw new InvalidOperationException();

            Marshal.Copy(buffer, 0, memory, structSize);

            return (T)Marshal.PtrToStructure(memory, typeof(T))!;
        }
        finally
        {
            if (memory != IntPtr.Zero)
                Marshal.FreeCoTaskMem(memory);
        }
    }

    private const ushort IMAGE_DOS_SIGNATURE = 0x5A4D;  // MZ
    private const uint IMAGE_NT_SIGNATURE = 0x00004550; // PE00

    private const ushort IMAGE_FILE_MACHINE_I386 = 0x014C;
    private const ushort IMAGE_FILE_MACHINE_IA64 = 0x0200;
    private const ushort IMAGE_FILE_MACHINE_AMD64 = 0x8664;

    private const ushort IMAGE_NT_OPTIONAL_HDR32_MAGIC = 0x10B;
    private const ushort IMAGE_NT_OPTIONAL_HDR64_MAGIC = 0x20B;

    private const ushort IMAGE_FILE_DLL = 0x2000;
}
