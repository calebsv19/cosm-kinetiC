"""GCC/Clang dependency grammar and exact repeated-header regression."""
import shutil
import subprocess
import tempfile
import unittest
from pathlib import Path

import sys
sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "scripts"))
import build_outputs


class GeneratedRules(unittest.TestCase):
    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory()
        self.addCleanup(self.temporary.cleanup)
        self.repo = Path(self.temporary.name).resolve()
        self.dep = self.repo / 'build/probe.d'

    def parse(self, text):
        return build_outputs.dependency_syntax(self.repo, self.dep, text)

    def test_exact_repeated_app_config_rule(self):
        text = ('build/probe.o: src/probe.c include/app/app_config.h \\\n'
                ' include/app/app_config.h\n'
                'include/app/app_config.h:\ninclude/app/app_config.h:\n')
        self.assertEqual(self.parse(text), sorted([
            self.repo / 'src/probe.c', self.repo / 'include/app/app_config.h']))

    def test_repetition_adds_no_new_dependency(self):
        base = 'build/probe.o: src/probe.c include/value.h\ninclude/value.h:\n'
        self.assertEqual(self.parse(base), self.parse(base + 'include/value.h:\n' * 20))

    def test_unknown_empty_or_repeated_rule_refused(self):
        for extra in ('foreign.h:\n', 'foreign.h:\nforeign.h:\n', '.PHONY:\n'):
            with self.subTest(extra=extra), self.assertRaises(ValueError):
                self.parse('build/probe.o: src/probe.c\n' + extra)

    def test_known_rule_with_prerequisites_refused(self):
        for extra in ('include/value.h: foreign.h\n', 'include/value.h: include/value.h\n'):
            with self.subTest(extra=extra), self.assertRaises(ValueError):
                self.parse('build/probe.o: src/probe.c include/value.h\ninclude/value.h:\n' + extra)

    def test_recipe_or_make_expression_still_refused(self):
        for extra in ('\ttouch escaped\n', '$(shell touch escaped):\n',
                      'include/value.h: ; touch escaped\n', 'include/value.h: | foreign\n',
                      'include foreign.mk\n', 'include/value.h::\n'):
            with self.subTest(extra=extra), self.assertRaises(ValueError):
                self.parse('build/probe.o: src/probe.c include/value.h\n' + extra)

    def test_wrong_object_and_empty_closure_refused(self):
        for text in ('build/other.o: src/probe.c\n', 'build/probe.o:\n'):
            with self.subTest(text=text), self.assertRaises(ValueError): self.parse(text)

    def test_compiler_forms_and_escaped_spaces(self):
        (self.repo / 'src').mkdir()
        (self.repo / 'build').mkdir()
        (self.repo / 'src/value space.h').write_text('#define VALUE 3\n')
        (self.repo / 'src/probe.c').write_text(
            '#include "value space.h"\n#include "value space.h"\n'
            '#include <stddef.h>\nint value(void) { return VALUE; }\n')
        compilers = [path for name in ('clang', 'gcc-16', 'gcc-15', 'gcc-14', 'gcc')
                     if (path := shutil.which(name))]
        self.assertTrue(compilers)
        for compiler in dict.fromkeys(compilers):
            for mode in ('-MM', '-M', '-MMD', '-MD'):
                with self.subTest(compiler=compiler, mode=mode):
                    command = [compiler, mode, '-MP', '-MF', str(self.dep),
                               '-MQ', 'build/probe.o', 'src/probe.c']
                    if mode in ('-MMD', '-MD'):
                        command += ['-c', '-o', 'build/probe.o']
                    result = subprocess.run(command, cwd=self.repo, capture_output=True,
                                            text=True, timeout=30)
                    self.assertEqual(result.returncode, 0, result.stderr)
                    inputs = self.parse(self.dep.read_text())
                    self.assertIn(self.repo / 'src/value space.h', inputs)
                    self.assertIn(self.repo / 'src/probe.c', inputs)
                    self.assertEqual(len(inputs), len(set(inputs)))


if __name__ == '__main__': unittest.main()
