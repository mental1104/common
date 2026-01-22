import pytest

from mental1104.utils.batch_rename import (
    RenameOp,
    apply_rename_plan,
    build_indexed_rename_plan,
    plan_directory_rename,
    rename_with_index,
    rename_with_regex_group,
    rename_with_suffix,
)


class TestBatchRename:
    def test_plan_directory_rename_suffix(self, tmp_path):
        """
        Rename plan should replace the suffix while keeping the stem.
        """
        src = tmp_path / "clip.wav"
        src.write_text("audio")

        plan = plan_directory_rename(tmp_path, rename_with_suffix(".m4a"))

        assert plan == [RenameOp(src=src, dst=tmp_path / "clip.m4a")]

    def test_plan_directory_rename_regex_group(self, tmp_path):
        """
        Regex-based rule should extract the group and skip non-matching files.
        """
        hit = tmp_path / "[001].mp4"
        miss = tmp_path / "intro.mp4"
        hit.write_text("hit")
        miss.write_text("miss")

        rule = rename_with_regex_group(r"\[(\d{3})\]", suffix=".mp4")
        plan = plan_directory_rename(tmp_path, rule, sort_key=lambda path: path.name)

        assert len(plan) == 1
        assert plan[0].src == hit
        assert plan[0].dst == tmp_path / "001.mp4"

    def test_apply_rename_plan_swap(self, tmp_path):
        """
        Two-phase apply should support swapping file names without conflicts.
        """
        file_a = tmp_path / "a.txt"
        file_b = tmp_path / "b.txt"
        file_a.write_text("alpha")
        file_b.write_text("beta")

        plan = [
            RenameOp(src=file_a, dst=tmp_path / "b.txt"),
            RenameOp(src=file_b, dst=tmp_path / "a.txt"),
        ]

        apply_rename_plan(plan)

        assert (tmp_path / "a.txt").read_text() == "beta"
        assert (tmp_path / "b.txt").read_text() == "alpha"

    def test_apply_rename_plan_conflict_raises(self, tmp_path):
        """
        Existing destinations outside the plan should be rejected by default.
        """
        file_a = tmp_path / "a.txt"
        file_b = tmp_path / "b.txt"
        file_a.write_text("alpha")
        file_b.write_text("beta")

        plan = [RenameOp(src=file_a, dst=file_b)]

        with pytest.raises(FileExistsError):
            apply_rename_plan(plan)

    def test_build_indexed_rename_plan(self, tmp_path):
        """
        Indexed rename rule should generate deterministic sequence names.
        """
        first = tmp_path / "first.txt"
        second = tmp_path / "second.txt"
        first.write_text("first")
        second.write_text("second")

        rule = rename_with_index(start=1, width=3, suffix=".dat")
        plan = build_indexed_rename_plan([first, second], rule)

        assert [op.dst.name for op in plan] == ["001.dat", "002.dat"]
