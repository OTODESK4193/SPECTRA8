import docx
import os
import glob

docs_dir = r"D:\VST_Project\SPECTRA8\Docs"
for fpath in glob.glob(os.path.join(docs_dir, "*.docx")):
    doc = docx.Document(fpath)
    txt_path = fpath.replace(".docx", ".txt")
    with open(txt_path, "w", encoding="utf-8") as f:
        for para in doc.paragraphs:
            f.write(para.text + "\n")
    print(f"OK: {os.path.basename(fpath)}")
print("All done")
