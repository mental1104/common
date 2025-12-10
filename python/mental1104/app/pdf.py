from pypdf import PdfReader, PdfWriter


def extract_page_range(
    src_pdf: str,
    dst_pdf: str,
    start_page: int,
    end_page: int,
) -> None:
    """
    Extract pages [start_page, end_page] (1-based, inclusive)
    from src_pdf into dst_pdf.
    """
    reader = PdfReader(src_pdf)
    writer = PdfWriter()

    total_pages = len(reader.pages)
    if start_page < 1 or end_page < start_page:
        raise ValueError("Invalid page range")

    # clamp to valid range
    start_idx = start_page - 1
    end_idx = min(end_page, total_pages)

    for i in range(start_idx, end_idx):
        writer.add_page(reader.pages[i])

    with open(dst_pdf, "wb") as f:
        writer.write(f)
