import os
import subprocess
import re
import time
import blessed
from concurrent.futures import ProcessPoolExecutor, as_completed

INPUTS_DIR = "inputs"
OUTPUTS_DIR = "outputs"
RESULTS_FILE = "results.log"

if os.name == 'nt':
    MAIN_EXECUTABLE = "main.exe"
    CHECKER_EXECUTABLE = "format_checker.exe"
else:
    MAIN_EXECUTABLE = "./main"
    CHECKER_EXECUTABLE = "./format_checker"


def compile_project(term):
    print(term.bold("--- Step 1: Compiling Project ---"))
    commands = {
        "Cleaning": ["make", "clean"],
        "Compiling Main": ["make"],
        "Compiling Checker": ["make", "checker"]
    }

    for description, command in commands.items():
        print(f"Running '{' '.join(command)}'...", end='', flush=True)
        try:
            subprocess.run(
                command,
                check=True,
                capture_output=True,
                text=True
            )
            print(term.green(" OK"))
        except FileNotFoundError:
            print(term.red(" FAILED"))
            print(term.red(f"\nError: '{command[0]}' command not found. Is make installed and in your PATH?"))
            return False
        except subprocess.CalledProcessError as e:
            print(term.red(" FAILED"))
            print(term.red("\n--- Compiler Error ---"))
            print(e.stderr)
            print(term.red("----------------------"))
            return False

    print(term.green("Compilation successful.\n"))
    return True


def check_existing_output(tc_name):
    input_path = os.path.join(INPUTS_DIR, tc_name)
    output_path = os.path.join(OUTPUTS_DIR, tc_name)

    if not os.path.exists(output_path):
        return -1, "Output Missing"

    try:
        checker_result = subprocess.run(
            [CHECKER_EXECUTABLE, input_path, output_path],
            capture_output=True, text=True, timeout=30
        )
    except FileNotFoundError:
        return -1, "Checker Not Found"
    except subprocess.TimeoutExpired:
        return -1, "Checker Timeout"

    checker_output = checker_result.stdout
    if "All constraints satisfied" in checker_output:
        score_match = re.search(r"FINAL SCORE:\s*(-?[\d.eE+-]+)", checker_output)
        if score_match:
            score = float(score_match.group(1))
            status = "Scored"
        else:
            score = -1
            status = "Score Not Found"
    else:
        score = -1
        status = "Constraints Failed"

    return score, status


def run_testcase(tc_name, _unused):
    """Run a single testcase and return (score, status)."""
    input_path = os.path.join(INPUTS_DIR, tc_name)
    output_path = os.path.join(OUTPUTS_DIR, tc_name)

    try:
        with open(input_path, 'r') as f:
            time_limit_minutes = int(f.readline().strip())
            time_limit_seconds = time_limit_minutes * 60
    except (IOError, ValueError):
        return -1, "Read Error"

    start_time = time.time()
    try:
        main_process = subprocess.Popen(
            [MAIN_EXECUTABLE, input_path, output_path],
            stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL
        )
        main_process.wait(timeout=time_limit_seconds)
    except FileNotFoundError:
        return -1, "Exec Not Found"
    except subprocess.TimeoutExpired:
        main_process.kill()
        return 0, f"Timeout ({time_limit_seconds}s)"
    except Exception:
        return -1, "Runtime Error"

    run_duration = time.time() - start_time
    score, status = check_existing_output(tc_name)
    if status == "Scored":
        status = f"Scored in {run_duration:.2f}s"
    return score, status


def print_final_table(term, results):
    print(term.bold(f"\n--- Final Results --- (Full log in {RESULTS_FILE})"))
    print("-" * 60)
    print(f"{'Test Case':<25} | {'Score':>15} | {'Status'}")
    print("-" * 60)

    total_score = 0
    for res in results:
        score = res['score']
        if score > 0:
            total_score += score

        status_color = term.green
        if res['status'].startswith("Timeout") or res['status'] in ['Output Missing']:
            status_color = term.yellow
        elif not res['status'].startswith("Scored"):
            status_color = term.red

        score_str = f"{score:15.2f}"
        if score == -1:
            score_str = term.red(f"{score:15.2f}")
        elif score == 0:
            score_str = term.yellow(f"{score:15.2f}")

        print(f"{res['tc']:<25} | {score_str} | " + status_color(res['status']))

    print("-" * 60)
    print(f"{term.bold('TOTAL SCORE'):<32} | {term.bold_green(f'{total_score:15,.2f}')} |")
    print("-" * 60)


def run_all_testcases_concurrently(term, selected):
    print(term.bold(f"--- Step 2: Running {len(selected)} Selected Test Cases (Concurrent) ---"))

    results = []
    with open(RESULTS_FILE, 'w') as log_file:
        log_file.write("Test Case, Score, Status\n")

    with ProcessPoolExecutor(max_workers=os.cpu_count()) as executor:
        futures = {executor.submit(run_testcase, tc_name, None): tc_name for tc_name in sorted(selected)}

        for future in as_completed(futures):
            tc_name = futures[future]
            try:
                score, status = future.result()
            except Exception as e:
                score, status = -1, f"Error: {e}"

            print(f"[{tc_name}] -> {status} | Score: {score}")
            results.append({"tc": tc_name, "score": score, "status": status})

            with open(RESULTS_FILE, 'a') as log_file:
                log_file.write(f"{tc_name}, {score}, {status}\n")

    print_final_table(term, results)


def main():
    term = blessed.Terminal()
    if not os.path.isdir(INPUTS_DIR):
        print(term.red(f"Error: Input directory '{INPUTS_DIR}' not found."))
        return

    testcases = sorted([f for f in os.listdir(INPUTS_DIR) if f.endswith(".txt")])
    if not testcases:
        print(term.yellow(f"No testcases found in '{INPUTS_DIR}'."))
        return

    if not compile_project(term):
        return

    os.makedirs(OUTPUTS_DIR, exist_ok=True)
    selected = set(testcases)

    run_all_testcases_concurrently(term, selected)


if __name__ == "__main__":
    main()

