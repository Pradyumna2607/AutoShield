# Contributing to AutoShield

Thanks for your interest! Contributions of all kinds are welcome, especially **field-test data**, threshold tuning results, and hardware improvements.

## Ways to contribute
- Report bugs or suggest features via issues (templates provided)
- Share field-test results using the *Field test report* issue template
- Improve firmware, simulation or documentation via pull requests

## Workflow
1. Fork the repo and create a branch: `git checkout -b feature/short-description`
2. Make your change and add or update tests
3. Run the checks below
4. Open a pull request using the PR template

## Checks
```bash
# Python simulation
cd simulation && pip install -r requirements.txt && pytest -q && cd ..
# Firmware logic (native)
g++ -std=c++11 -Wall -Wextra -o /tmp/t firmware/test/test_trigger_logic.cpp && /tmp/t
```
Any change to the trigger logic must be made in **both** `firmware/autoshield/trigger_logic.h` and `simulation/trigger_logic.py`, with tests passing in both.

## Style
- C++: Arduino-friendly, no dynamic allocation in the trigger logic
- Python: PEP 8, max line length 120
- Commit messages: short and imperative (`fix:`, `feat:`, `docs:`, `test:` prefixes welcome)

## Safety
Do not submit changes that weaken safety features (limit switches, timeout, manual override) without discussion. See [SAFETY.md](SAFETY.md).
