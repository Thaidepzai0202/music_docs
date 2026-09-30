# Big Rusty Drums (Karoryfer Samples) — CC0 1.0 (public domain)

Dùng cho: `content/kits/kit_acoustic/` (Acoustic / "Trống thật"). Kiểm license ngày 30/09/2026 tại nguồn chính thức.

## Ghi công (CC0 không bắt buộc — vẫn hiện ở màn "Giấy phép" của app)

> Acoustic drum samples from **Big Rusty Drums** by **Karoryfer Samples** — released under CC0 1.0.
> <https://shop.karoryfer.com/pages/free-big-rusty-drums>

## Nguồn

| | |
|---|---|
| Trang chính thức | <https://shop.karoryfer.com/pages/free-big-rusty-drums> — "Big Rusty Drums" miễn phí; Karoryfer phát hành các thư viện miễn phí dưới CC0 |
| Repo đã tải | <https://github.com/sfzinstruments/karoryfer.big-rusty-drums> (nhánh `master`), file `LICENSE` = "CC0 1.0 Universal" (GitHub: CC0-1.0) |
| Bộ trống | Trống cổ Ba Lan của xưởng Zygmunt Szpaderski: kick 24", snare 14", tom 14" / 15" / 18" / 22", hi-hat 14", crash 17", ride 22" |
| File đã dùng | 74 file FLAC 44.1 kHz (danh sách: `engine/tools/library_sources.tsv`): kick, snare center / sidestick / rim click / rimshot, 4 tom, hi-hat đóng / chân / mở, crash, ride — mỗi âm 2 lớp velocity (lớp giữa + mạnh nhất), round-robin 1, mọi micro của âm đó |

## Thay đổi

Rút gọn bằng `engine/tools/le-import-library --kit-only` (render/SampleImport.cpp, `importKit`):
- trộn các micro thành mono (kick: mic trong trống + 0.35 overhead; snare: top + 0.4 bottom + 0.6 overhead; tom / hi-hat:
  close + 0.6–0.7 overhead; cymbal: 0.4 close + overhead), resample 44.1 → 48 kHz;
- cắt khoảng lặng đầu, cắt đuôi ở −60 dB hoặc tối đa 0.5–4 s tuỳ âm, fade-out cosin 25 % cuối;
- chuẩn hoá đỉnh −1 dBFS; pad nhỏ hơn trung vị loudness của kit được nén đỉnh ≤ 6 dB; cân loudness bằng SFZ `volume`;
- map GM 36–51: pad 39 dùng **Rim Click** (bộ này không có clap), pad 40 **Rimshot**, pad 48 / 50 dùng lại tom 14"
  nâng 3 / 6 nửa cung bằng SFZ `tune` (bộ chỉ có 4 tom); ghi FLAC 24-bit 48 kHz, SFZ viết mới.

## Nguyên văn — LICENSE (repo sfzinstruments/karoryfer.big-rusty-drums)

```
Creative Commons Legal Code

CC0 1.0 Universal

    CREATIVE COMMONS CORPORATION IS NOT A LAW FIRM AND DOES NOT PROVIDE
    LEGAL SERVICES. DISTRIBUTION OF THIS DOCUMENT DOES NOT CREATE AN
    ATTORNEY-CLIENT RELATIONSHIP. CREATIVE COMMONS PROVIDES THIS
    INFORMATION ON AN "AS-IS" BASIS. CREATIVE COMMONS MAKES NO WARRANTIES
    REGARDING THE USE OF THIS DOCUMENT OR THE INFORMATION OR WORKS
    PROVIDED HEREUNDER, AND DISCLAIMS LIABILITY FOR DAMAGES RESULTING FROM
    THE USE OF THIS DOCUMENT OR THE INFORMATION OR WORKS PROVIDED
    HEREUNDER.

Statement of Purpose

The laws of most jurisdictions throughout the world automatically confer
exclusive Copyright and Related Rights (defined below) upon the creator
and subsequent owner(s) (each and all, an "owner") of an original work of
authorship and/or a database (each, a "Work").

Certain owners wish to permanently relinquish those rights to a Work for
the purpose of contributing to a commons of creative, cultural and
scientific works ("Commons") that the public can reliably and without fear
of later claims of infringement build upon, modify, incorporate in other
works, reuse and redistribute as freely as possible in any form whatsoever
and for any purposes, including without limitation commercial purposes.
These owners may contribute to the Commons to promote the ideal of a free
culture and the further production of creative, cultural and scientific
works, or to gain reputation or greater distribution for their Work in
part through the use and efforts of others.

For these and/or other purposes and motivations, and without any
expectation of additional consideration or compensation, the person
associating CC0 with a Work (the "Affirmer"), to the extent that he or she
is an owner of Copyright and Related Rights in the Work, voluntarily
elects to apply CC0 to the Work and publicly distribute the Work under its
terms, with knowledge of his or her Copyright and Related Rights in the
Work and the meaning and intended legal effect of CC0 on those rights.

1. Copyright and Related Rights. A Work made available under CC0 may be
protected by copyright and related or neighboring rights ("Copyright and
Related Rights"). Copyright and Related Rights include, but are not
limited to, the following:

  i. the right to reproduce, adapt, distribute, perform, display,
     communicate, and translate a Work;
 ii. moral rights retained by the original author(s) and/or performer(s);
iii. publicity and privacy rights pertaining to a person's image or
     likeness depicted in a Work;
 iv. rights protecting against unfair competition in regards to a Work,
     subject to the limitations in paragraph 4(a), below;
  v. rights protecting the extraction, dissemination, use and reuse of data
     in a Work;
 vi. database rights (such as those arising under Directive 96/9/EC of the
     European Parliament and of the Council of 11 March 1996 on the legal
     protection of databases, and under any national implementation
     thereof, including any amended or successor version of such
     directive); and
vii. other similar, equivalent or corresponding rights throughout the
     world based on applicable law or treaty, and any national
     implementations thereof.

2. Waiver. To the greatest extent permitted by, but not in contravention
of, applicable law, Affirmer hereby overtly, fully, permanently,
irrevocably and unconditionally waives, abandons, and surrenders all of
Affirmer's Copyright and Related Rights and associated claims and causes
of action, whether now known or unknown (including existing as well as
future claims and causes of action), in the Work (i) in all territories
worldwide, (ii) for the maximum duration provided by applicable law or
treaty (including future time extensions), (iii) in any current or future
medium and for any number of copies, and (iv) for any purpose whatsoever,
including without limitation commercial, advertising or promotional
purposes (the "Waiver"). Affirmer makes the Waiver for the benefit of each
member of the public at large and to the detriment of Affirmer's heirs and
successors, fully intending that such Waiver shall not be subject to
revocation, rescission, cancellation, termination, or any other legal or
equitable action to disrupt the quiet enjoyment of the Work by the public
as contemplated by Affirmer's express Statement of Purpose.

3. Public License Fallback. Should any part of the Waiver for any reason
be judged legally invalid or ineffective under applicable law, then the
Waiver shall be preserved to the maximum extent permitted taking into
account Affirmer's express Statement of Purpose. In addition, to the
extent the Waiver is so judged Affirmer hereby grants to each affected
person a royalty-free, non transferable, non sublicensable, non exclusive,
irrevocable and unconditional license to exercise Affirmer's Copyright and
Related Rights in the Work (i) in all territories worldwide, (ii) for the
maximum duration provided by applicable law or treaty (including future
time extensions), (iii) in any current or future medium and for any number
of copies, and (iv) for any purpose whatsoever, including without
limitation commercial, advertising or promotional purposes (the
"License"). The License shall be deemed effective as of the date CC0 was
applied by Affirmer to the Work. Should any part of the License for any
reason be judged legally invalid or ineffective under applicable law, such
partial invalidity or ineffectiveness shall not invalidate the remainder
of the License, and in such case Affirmer hereby affirms that he or she
will not (i) exercise any of his or her remaining Copyright and Related
Rights in the Work or (ii) assert any associated claims and causes of
action with respect to the Work, in either case contrary to Affirmer's
express Statement of Purpose.

4. Limitations and Disclaimers.

 a. No trademark or patent rights held by Affirmer are waived, abandoned,
    surrendered, licensed or otherwise affected by this document.
 b. Affirmer offers the Work as-is and makes no representations or
    warranties of any kind concerning the Work, express, implied,
    statutory or otherwise, including without limitation warranties of
    title, merchantability, fitness for a particular purpose, non
    infringement, or the absence of latent or other defects, accuracy, or
    the present or absence of errors, whether or not discoverable, all to
    the greatest extent permissible under applicable law.
 c. Affirmer disclaims responsibility for clearing rights of other persons
    that may apply to the Work or any use thereof, including without
    limitation any person's Copyright and Related Rights in the Work.
    Further, Affirmer disclaims responsibility for obtaining any necessary
    consents, permissions or other rights required for any use of the
    Work.
 d. Affirmer understands and acknowledges that Creative Commons is not a
    party to this document and has no duty or obligation with respect to
    this CC0 or use of the Work.
```
