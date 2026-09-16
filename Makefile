CC ?= gcc
CXX ?= g++
PYTHON ?= python3
CFLAGS ?= -std=c11 -O2 -Wall -Wextra -Werror
LFLAGS ?= -lm

EXPANDED = -DTHREADBOBBER_PROFILE_EXPANDED
TESTS = tests/test.c tests/vmtest_program.c tests/features_program.c

all: test_compact test_expanded

test_compact: $(TESTS) threadbobber.h tests/utest.h
	$(CC) $(CFLAGS) -I. -Itests $(TESTS) -o $@ $(LFLAGS)

test_expanded: $(TESTS) threadbobber.h tests/utest.h
	$(CC) $(CFLAGS) $(EXPANDED) -I. -Itests $(TESTS) -o $@ $(LFLAGS)

# Runs the tests in both profiles and checks that the header compiles as C++.
.PHONY: test
test: test_compact test_expanded
	./test_compact
	./test_expanded
	$(CXX) -std=c++11 -pedantic-errors -DTHREADBOBBER_IMPLEMENTATION -x c++ -fsyntax-only -I. threadbobber.h
	$(CXX) -std=c++11 -pedantic-errors $(EXPANDED) -DTHREADBOBBER_IMPLEMENTATION -x c++ -fsyntax-only -I. threadbobber.h

# Compares traces with the official runtime and exercises the converter end to
# end. Needs YarnSpinner.Console 3.2.2 (`dotnet tool install -g
# YarnSpinner.Console --version 3.2.2`) and .NET 10.
.PHONY: parity
parity:
	$(PYTHON) tests/parity.py --cc $(CC)
	$(PYTHON) -m unittest discover -s tests -p "test_*.py"

# Regenerates the checked-in test programs. Needs ysc 3.2.2.
.PHONY: generate
generate:
	$(PYTHON) threadbobber.py tests/vmtest.yarnproject --out tests --prefix vmtest
	$(PYTHON) threadbobber.py tests/features.yarnproject --out tests --prefix features

.PHONY: clean
clean:
	$(RM) test_compact test_expanded *.exe
