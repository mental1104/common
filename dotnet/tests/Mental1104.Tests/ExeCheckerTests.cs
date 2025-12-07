using System;
using System.Collections.Generic;
using System.IO;
using System.Runtime.InteropServices;
using Mental1104.Executables;
using Xunit;

namespace Mental1104.Tests.Executables;

public class ExeCheckerTests
{
    private const int NtHeaderOffset = 0x80;

    [Fact]
    public void ReturnsFalseWhenFileIsMissing()
    {
        string missingPath = Path.Combine(Path.GetTempPath(), Guid.NewGuid().ToString("N") + ".exe");
        Assert.False(ExeChecker.IsValidExe(missingPath));
    }

    [Fact]
    public void AcceptsValid32BitExe()
    {
        byte[] content = BuildPeFile(IMAGE_FILE_MACHINE_I386, IMAGE_NT_OPTIONAL_HDR32_MAGIC);
        using var file = new TempFile(content);

        Assert.True(ExeChecker.IsValidExe(file.Path));
    }

    [Fact]
    public void AcceptsValid64BitExe()
    {
        byte[] content = BuildPeFile(IMAGE_FILE_MACHINE_AMD64, IMAGE_NT_OPTIONAL_HDR64_MAGIC);
        using var file = new TempFile(content);

        Assert.True(ExeChecker.IsValidExe(file.Path));
    }

    [Fact]
    public void RejectsDllFiles()
    {
        byte[] content = BuildPeFile(IMAGE_FILE_MACHINE_AMD64, IMAGE_NT_OPTIONAL_HDR64_MAGIC, markAsDll: true);
        using var file = new TempFile(content);

        Assert.False(ExeChecker.IsValidExe(file.Path));
    }

    [Fact]
    public void RejectsInvalidOptionalHeaderFor32Bit()
    {
        byte[] content = BuildPeFile(IMAGE_FILE_MACHINE_I386, IMAGE_NT_OPTIONAL_HDR64_MAGIC);
        using var file = new TempFile(content);

        Assert.False(ExeChecker.IsValidExe(file.Path));
    }

    [Fact]
    public void RejectsInvalidOptionalHeaderFor64Bit()
    {
        byte[] content = BuildPeFile(IMAGE_FILE_MACHINE_AMD64, IMAGE_NT_OPTIONAL_HDR32_MAGIC);
        using var file = new TempFile(content);

        Assert.False(ExeChecker.IsValidExe(file.Path));
    }

    [Fact]
    public void RejectsInvalidNtSignature()
    {
        byte[] content = BuildPeFile(IMAGE_FILE_MACHINE_I386, IMAGE_NT_OPTIONAL_HDR32_MAGIC, ntSignature: 0);
        using var file = new TempFile(content);

        Assert.False(ExeChecker.IsValidExe(file.Path));
    }

    [Fact]
    public void RejectsInvalidDosSignature()
    {
        byte[] content = BuildPeFile(IMAGE_FILE_MACHINE_I386, IMAGE_NT_OPTIONAL_HDR32_MAGIC, dosSignature: 0x1234);
        using var file = new TempFile(content);

        Assert.False(ExeChecker.IsValidExe(file.Path));
    }

    [Fact]
    public void RejectsUnknownMachine()
    {
        const ushort unknownMachine = 0xFFFF;
        byte[] content = BuildPeFile(unknownMachine, IMAGE_NT_OPTIONAL_HDR32_MAGIC);
        using var file = new TempFile(content);

        Assert.False(ExeChecker.IsValidExe(file.Path));
    }

    private static byte[] BuildPeFile(
        ushort machine,
        ushort optionalMagic,
        bool markAsDll = false,
        uint? ntSignature = null,
        ushort? dosSignature = null)
    {
        ushort resolvedDosSignature = dosSignature ?? IMAGE_DOS_SIGNATURE;
        uint resolvedNtSignature = ntSignature ?? IMAGE_NT_SIGNATURE;

        var dosHeader = new IMAGE_DOS_HEADER
        {
            e_magic = resolvedDosSignature,
            e_lfanew = NtHeaderOffset
        };

        var fileHeader = new IMAGE_FILE_HEADER
        {
            Machine = machine,
            Characteristics = markAsDll ? IMAGE_FILE_DLL : (ushort)0,
            SizeOfOptionalHeader = (ushort)(Is64BitMachine(machine)
                ? Marshal.SizeOf(typeof(IMAGE_OPTIONAL_HEADER64))
                : Marshal.SizeOf(typeof(IMAGE_OPTIONAL_HEADER32)))
        };

        var bytes = new List<byte>();
        bytes.AddRange(ToBytes(dosHeader));

        if (bytes.Count < NtHeaderOffset)
            bytes.AddRange(new byte[NtHeaderOffset - bytes.Count]);

        if (Is64BitMachine(machine))
        {
            var header = new IMAGE_NT_HEADERS64
            {
                Signature = resolvedNtSignature,
                FileHeader = fileHeader,
                OptionalHeader = new IMAGE_OPTIONAL_HEADER64
                {
                    Magic = optionalMagic
                }
            };
            bytes.AddRange(ToBytes(header));
        }
        else
        {
            var header = new IMAGE_NT_HEADERS32
            {
                Signature = resolvedNtSignature,
                FileHeader = fileHeader,
                OptionalHeader = new IMAGE_OPTIONAL_HEADER32
                {
                    Magic = optionalMagic
                }
            };
            bytes.AddRange(ToBytes(header));
        }

        return bytes.ToArray();
    }

    private static bool Is64BitMachine(ushort machine) =>
        machine == IMAGE_FILE_MACHINE_AMD64 || machine == IMAGE_FILE_MACHINE_IA64;

    private static byte[] ToBytes<T>(T value)
    {
        int size = Marshal.SizeOf(typeof(T));
        IntPtr ptr = Marshal.AllocCoTaskMem(size);

        try
        {
            Marshal.StructureToPtr(value, ptr, false);
            var buffer = new byte[size];
            Marshal.Copy(ptr, buffer, 0, size);
            return buffer;
        }
        finally
        {
            Marshal.FreeCoTaskMem(ptr);
        }
    }

    private sealed class TempFile : IDisposable
    {
        public string Path { get; }

        public TempFile(byte[] content)
        {
            Path = System.IO.Path.Combine(System.IO.Path.GetTempPath(), System.Guid.NewGuid().ToString("N") + ".exe");
            File.WriteAllBytes(Path, content);
        }

        public void Dispose()
        {
            if (File.Exists(Path))
                File.Delete(Path);
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

    [StructLayout(LayoutKind.Sequential)]
    private struct IMAGE_DOS_HEADER
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
    private struct IMAGE_FILE_HEADER
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
    private struct IMAGE_NT_HEADERS32
    {
        public uint Signature;
        public IMAGE_FILE_HEADER FileHeader;
        public IMAGE_OPTIONAL_HEADER32 OptionalHeader;
    }

    [StructLayout(LayoutKind.Sequential)]
    private struct IMAGE_NT_HEADERS64
    {
        public uint Signature;
        public IMAGE_FILE_HEADER FileHeader;
        public IMAGE_OPTIONAL_HEADER64 OptionalHeader;
    }

    [StructLayout(LayoutKind.Sequential)]
    private struct IMAGE_OPTIONAL_HEADER32
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
    private struct IMAGE_OPTIONAL_HEADER64
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
}
