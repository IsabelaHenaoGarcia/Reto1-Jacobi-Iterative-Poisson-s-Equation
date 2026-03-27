#!/usr/bin/env bash
# =============================================================================
# benchmark.sh  --  Poisson 1D Jacobi: convergence-based speedup study
#
# All binaries run until RMS residual < 1e-6. Speedup is always computed as
# T(serial_std, n) / T(impl, n, p), read from data_serial.csv.
#
# One CSV per suite (mirrors the structure of the previous benchmark):
#   results/data_serial.csv    -- serial_std, serial_opt, serial_cache
#   results/data_threads.csv   -- threads(p=2,4,6,8)
#   results/data_processes.csv -- processes(p=2,4,6,8), n <= PROC_MAX_N
#
# CSV columns (all suites):
#   suite, impl, parallelism, grid_size, repetition, wall_time_ms, iters_done
#
# Grid sizes:
#   serial / threads  : 100 200 500 1000 2000
#   processes         : 100 200 500  (fork-per-iteration overhead cap)
#
# Parallel counts (same set for threads and processes — required correlation):
#   p = 2 4 6 8 12
#
# Usage:
#   sudo ./benchmark.sh [all|serial|threads|procs|summary]
# =============================================================================

set -euo pipefail
export LC_NUMERIC=C

SCRIPT_DIR=$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)
RESULTS_DIR="results"

GRID_SIZES=(100 200 500 1000 2000)
PROC_MAX_N=2000
PARALLEL_COUNTS=(2 4 6 8 12)
MAX_ITERS=20000000
REPETITIONS=10 

CSV_SERIAL="${RESULTS_DIR}/data_serial.csv"
CSV_THREADS="${RESULTS_DIR}/data_threads.csv"
CSV_PROCS="${RESULTS_DIR}/data_processes.csv"
CSV_HEADER="suite,impl,parallelism,grid_size,repetition,wall_time_ms,iters_done,max_error"

# ---------------------------------------------------------------------------
# Logging
# ---------------------------------------------------------------------------
BOLD="\033[1m"; GREEN="\033[0;32m"; RED="\033[0;31m"
CYAN="\033[0;36m"; RESET="\033[0m"

log_info()    { echo -e "  ${CYAN}>${RESET} $*"; }
log_ok()      { echo -e "  ${GREEN}[ok]${RESET} $*"; }
log_error()   { echo -e "  ${RED}[error]${RESET} $*" >&2; }
log_section() { echo -e "\n${BOLD}-- $* ${RESET}"; }

# ---------------------------------------------------------------------------
# System optimization / restore
# ---------------------------------------------------------------------------
_DM_UNIT=""

_detect_display_manager() {
    local candidates=(sddm gdm gdm3 lightdm ly greetd)
    for dm in "${candidates[@]}"; do
        systemctl is-active --quiet "${dm}.service" 2>/dev/null \
            && echo "${dm}.service" && return
    done
    echo ""
}

optimize_system() {
    if [[ "${EUID}" -ne 0 ]]; then
        log_error "Run as root to apply system optimizations (sudo ./benchmark.sh)"
        log_error "Continuing without optimizations."
        return
    fi

    log_section "Applying system optimizations"

    _DM_UNIT="$(_detect_display_manager)"
    if [[ -n "${_DM_UNIT}" ]]; then
        log_info "Stopping display manager: ${_DM_UNIT}"
        systemctl stop "${_DM_UNIT}" 2>/dev/null \
            && log_ok "Stopped" || log_error "Failed to stop ${_DM_UNIT}"
    else
        log_info "No active display manager detected"
    fi

    log_info "Isolating multi-user.target"
    systemctl isolate multi-user.target 2>/dev/null \
        && log_ok "OK" || log_error "Failed"

    log_info "Setting CPU governor: performance"
    local gov_ok=0
    for f in /sys/devices/system/cpu/cpu*/cpufreq/scaling_governor; do
        [[ -f "${f}" ]] && echo performance > "${f}" && gov_ok=1
    done
    (( gov_ok )) && log_ok "Done" || log_error "Could not set governor"

    if [[ -f /sys/devices/system/cpu/cpufreq/boost ]]; then
        echo 0 > /sys/devices/system/cpu/cpufreq/boost 2>/dev/null \
            && log_ok "AMD boost disabled" || log_error "Failed"
    elif [[ -f /sys/devices/system/cpu/intel_pstate/no_turbo ]]; then
        echo 1 > /sys/devices/system/cpu/intel_pstate/no_turbo 2>/dev/null \
            && log_ok "Intel turbo disabled" || log_error "Failed"
    else
        log_info "No boost control path found — skipping"
    fi

    log_ok "System ready"
}

restore_system() {
    [[ "${EUID}" -ne 0 ]] && return
    log_section "Restoring system"

    local gov_ok=0
    for f in /sys/devices/system/cpu/cpu*/cpufreq/scaling_governor; do
        [[ -f "${f}" ]] && echo powersave > "${f}" && gov_ok=1
    done
    (( gov_ok )) && log_ok "CPU governor restored to powersave"

    if [[ -f /sys/devices/system/cpu/cpufreq/boost ]]; then
        echo 1 > /sys/devices/system/cpu/cpufreq/boost 2>/dev/null \
            && log_ok "AMD boost re-enabled" || log_error "Failed"
    elif [[ -f /sys/devices/system/cpu/intel_pstate/no_turbo ]]; then
        echo 0 > /sys/devices/system/cpu/intel_pstate/no_turbo 2>/dev/null \
            && log_ok "Intel turbo re-enabled" || log_error "Failed"
    fi

    systemctl isolate graphical.target 2>/dev/null \
        && log_ok "Switched back to graphical.target" || log_error "Failed"
    git add . && \
    git commit -m "Upload results" && \
    git push
    [[ -n "${_DM_UNIT}" ]] && {
        systemctl start "${_DM_UNIT}" 2>/dev/null \
            && log_ok "Display manager started" || log_error "Failed"
    }
    log_ok "System restored"
}

# ---------------------------------------------------------------------------
# Pre-flight
# ---------------------------------------------------------------------------
check_and_compile() {
    local missing=0
    for bin in serial_std serial_opt serial_cache threads processes; do
        [[ ! -x "${SCRIPT_DIR}/bin/${bin}" ]] \
            && log_info "Binary missing: ${bin}" && missing=1
    done
    if (( missing )); then
        log_section "Compiling"
        [[ ! -f "${SCRIPT_DIR}/Makefile" ]] \
            && log_error "Makefile not found" && exit 1
        make -C "${SCRIPT_DIR}" 2>&1 | sed 's/^/  /'
        log_ok "Compilation complete"
    else
        log_info "All binaries present"
    fi
}

# ---------------------------------------------------------------------------
# CSV helpers
# ---------------------------------------------------------------------------
setup_csv() {
    local csv="$1"
    mkdir -p "${RESULTS_DIR}"
    if [[ ! -f "${csv}" ]]; then
        echo "${CSV_HEADER}" > "${csv}"
        log_info "Created: ${csv}"
    else
        log_info "Appending to existing: ${csv}"
    fi
}

row_exists() {
    local csv="$1" impl="$2" par="$3" size="$4" rep="$5"
    awk -F',' -v im="$impl" -v pa="$par" -v si="$size" -v re="$rep" \
        'NR>1 && $2==im && $3==pa && $4==si && $5==re { found=1 }
         END { print found+0 }' "${csv}" 2>/dev/null
}

write_row() {
    local csv="$1" suite="$2"
    shift 2
    # remaining args: impl parallelism grid_size repetition wall_time_ms iters_done
    printf '%s,%s,%s,%s,%s,%s,%s,%s\n' "${suite}" "$@" >> "${csv}"
    sync
}

# ---------------------------------------------------------------------------
# Single run — stdout: "wall_time_ms iters_done"
# All binaries emit "iters=N" in their stderr diagnostic line.
# ---------------------------------------------------------------------------
run_once() {
    local bin="$1" size="$2" par="$3"
    local ms iters stderr_out exit_code=0
    local errfile
    errfile=$(mktemp)

    if [[ "${par}" -eq 0 ]]; then
        ms=$("${bin}" "${size}" "${MAX_ITERS}" 2>"${errfile}") || exit_code=$?
    else
        ms=$("${bin}" "${size}" "${MAX_ITERS}" "${par}" 2>"${errfile}") || exit_code=$?
    fi

    stderr_out=$(cat "${errfile}"); rm -f "${errfile}"

    if [[ -z "${ms}" || "${exit_code}" -ne 0 ]]; then
        log_error "FAILED: ${bin} n=${size} p=${par} (exit=${exit_code})"
        echo "0.000 0"; return
    fi

    iters=$(echo "${stderr_out}" | grep -oP 'iters=\K[0-9]+'   | head -1 || echo "0")
    error=$(echo "${stderr_out}" | grep -oP 'error=\K[0-9.e+-]+' | head -1 || echo "0")
    echo "${ms} ${iters} ${error}"
}

# ---------------------------------------------------------------------------
# Measure one (impl, parallelism) across applicable grid sizes
# ---------------------------------------------------------------------------
measure_impl() {
    local csv="$1" suite="$2" impl="$3" bin="$4" par="$5" max_n="$6"

    local label="${impl}"
    [[ "${par}" -gt 0 ]] && label="${impl}(p=${par})"
    log_section "Measuring: ${label}  [max_n=${max_n}]"

    for rep in $(seq 1 "${REPETITIONS}"); do
        for size in "${GRID_SIZES[@]}"; do
            [[ "${size}" -gt "${max_n}" ]] && continue

            if [[ "$(row_exists "${csv}" "${impl}" "${par}" "${size}" "${rep}")" -gt 0 ]]; then
                log_info "[skip] ${label} n=${size} rep=${rep}"
                continue
            fi

            printf "    rep=%-2s  n=%-6s  " "${rep}" "${size}"
            local result ms iters error
            result=$(run_once "${bin}" "${size}" "${par}")
            ms=$(echo    "${result}" | cut -d' ' -f1)
            iters=$(echo "${result}" | cut -d' ' -f2)
            error=$(echo "${result}" | cut -d' ' -f3)
            printf "%-14s ms  iters=%-12s error=%s\n" "${ms}" "${iters}" "${error}"
            [[ "${iters}" -ge "${MAX_ITERS}" ]] && echo "    [DID NOT CONVERGE]"

            write_row "${csv}" "${suite}" \
                      "${impl}" "${par}" "${size}" "${rep}" "${ms}" "${iters}" "${error}"
        done
    done
}

# ---------------------------------------------------------------------------
# Suite runners
# ---------------------------------------------------------------------------
run_suite_serial() {
    local max_n="${GRID_SIZES[-1]}"
    setup_csv "${CSV_SERIAL}"
    measure_impl "${CSV_SERIAL}" "serial" "serial_std"   "./bin/serial_std"   0 "${max_n}"
    measure_impl "${CSV_SERIAL}" "serial" "serial_opt"   "./bin/serial_opt"   0 "${max_n}"
    measure_impl "${CSV_SERIAL}" "serial" "serial_cache" "./bin/serial_cache" 0 "${max_n}"
    print_summary_serial
}

run_suite_threads() {
    local max_n="${GRID_SIZES[-1]}"
    setup_csv "${CSV_THREADS}"
    for p in "${PARALLEL_COUNTS[@]}"; do
        measure_impl "${CSV_THREADS}" "threads" "threads" "./bin/threads" "${p}" "${max_n}"
    done
    print_summary_threads
}

run_suite_procs() {
    local max_n="${GRID_SIZES[-1]}"
    setup_csv "${CSV_PROCS}"
    for p in "${PARALLEL_COUNTS[@]}"; do
        measure_impl "${CSV_PROCS}" "processes" "processes" "./bin/processes" "${p}" "${max_n}"
    done
    print_summary_procs
}

# ---------------------------------------------------------------------------
# Summary helpers — Python for clean formatting
# Reference T(serial_std, n) always read from CSV_SERIAL.
# ---------------------------------------------------------------------------
_load_ref_avgs() {
    # Prints "size avg_ms" lines for serial_std from CSV_SERIAL
    python3 - "${CSV_SERIAL}" << 'PYEOF'
import sys, csv
from collections import defaultdict
rows = defaultdict(list)
with open(sys.argv[1]) as f:
    for r in csv.DictReader(f):
        if r['impl'] == 'serial_std' and int(r['parallelism']) == 0:
            rows[int(r['grid_size'])].append(float(r['wall_time_ms']))
for s, vals in sorted(rows.items()):
    print(s, sum(vals)/len(vals))
PYEOF
}

print_summary_serial() {
    local summary="${RESULTS_DIR}/summary_serial.txt"
    log_section "Serial summary"
    python3 - "${CSV_SERIAL}" "${GRID_SIZES[@]}" << 'PYEOF' | tee "${summary}"
import sys, csv, math
from collections import defaultdict

path  = sys.argv[1]
sizes = [int(x) for x in sys.argv[2:]]

rows = defaultdict(list)
with open(path) as f:
    for r in csv.DictReader(f):
        key = (r['impl'], int(r['parallelism']), int(r['grid_size']))
        rows[key].append((float(r['wall_time_ms']), int(r['iters_done'])))

def avg(v): return sum(v)/len(v) if v else None
def std(v):
    m = avg(v)
    return math.sqrt(sum((x-m)**2 for x in v)/len(v)) if v and m else 0.0

avgs = {k: (avg([d[0] for d in v]), avg([d[1] for d in v]), std([d[0] for d in v]))
        for k, v in rows.items()}

ref = {s: avgs.get(('serial_std', 0, s), (None,))[0] for s in sizes}

W, C = 22, 16
impls = [('serial_std',0), ('serial_opt',0), ('serial_cache',0)]

print()
print("=" * (W + C * len(sizes) + 10))
print("  Serial suite  |  tolerance = 1e-6  |  speedup = T(serial_std)/T(impl)")
print()

# Iterations
print(f"  {'Impl':<{W}}" + "".join(f"{'n='+str(s):>{C}}" for s in sizes))
print(f"  {'-'*(W+C*len(sizes))}")
for impl, par in impls:
    row = f"  {impl:<{W}}"
    for s in sizes:
        v = avgs.get((impl, par, s))
        row += f"{int(v[1]):>{C},}" if v and v[1] else f"{'—':>{C}}"
    print(row)

print()
# Time + speedup
print(f"  {'Impl':<{W}}" +
      "".join(f"{'n='+str(s)+' ms':>{C}}{'sp':>8}" for s in sizes))
print(f"  {'-'*(W+(C+8)*len(sizes))}")
for impl, par in impls:
    row = f"  {impl:<{W}}"
    for s in sizes:
        v = avgs.get((impl, par, s))
        r = ref.get(s)
        if v and v[0]:
            row += f"{v[0]:>{C},.1f}"
            sp = r/v[0] if r and v[0] > 0 else None
            row += f"{sp:>8.3f}x" if sp else f"{'1.000x':>8}"
        else:
            row += f"{'—':>{C}}{'—':>8}"
    print(row)
print("=" * (W + C * len(sizes) + 10))
PYEOF
    log_ok "Summary: ${summary}"
    log_ok "Raw data: ${CSV_SERIAL}"
}

print_summary_threads() {
    local summary="${RESULTS_DIR}/summary_threads.txt"
    log_section "Threads summary"
    python3 - "${CSV_THREADS}" "${CSV_SERIAL}" "${GRID_SIZES[@]}" << 'PYEOF' | tee "${summary}"
import sys, csv, math
from collections import defaultdict

t_path  = sys.argv[1]
s_path  = sys.argv[2]
sizes   = [int(x) for x in sys.argv[3:]]

def load(path):
    rows = defaultdict(list)
    with open(path) as f:
        for r in csv.DictReader(f):
            key = (r['impl'], int(r['parallelism']), int(r['grid_size']))
            rows[key].append((float(r['wall_time_ms']), int(r['iters_done'])))

    return rows

def avg(v): return sum(v)/len(v) if v else None

t_rows = load(t_path)
s_rows = load(s_path)
all_rows = {**s_rows, **t_rows}

avgs = {k: (avg([d[0] for d in v]), avg([d[1] for d in v]))
        for k, v in all_rows.items()}

ref  = {s: avgs.get(('serial_std', 0, s), (None,))[0] for s in sizes}
pars = sorted({k[1] for k in t_rows if k[1] > 0})

W, C = 22, 16
print()
print("=" * (W + C * len(sizes) + 10))
print("  Threads suite  |  tolerance = 1e-6  |  speedup = T(serial_std)/T(threads,p)")
print()

# Iterations (first row = serial_std for reference)
print(f"  {'Impl':<{W}}" + "".join(f"{'n='+str(s):>{C}}" for s in sizes))
print(f"  {'-'*(W+C*len(sizes))}")
for impl, par in [('serial_std',0)] + [('threads',p) for p in pars]:
    lbl = impl if par == 0 else f"threads(p={par})"
    row = f"  {lbl:<{W}}"
    for s in sizes:
        v = avgs.get((impl, par, s))
        row += f"{int(v[1]):>{C},}" if v and v[1] else f"{'—':>{C}}"
    print(row)

print()
# Time + speedup
print(f"  {'Impl':<{W}}" +
      "".join(f"{'n='+str(s)+' ms':>{C}}{'sp':>8}" for s in sizes) +
      f"{'Avg sp':>10}")
print(f"  {'-'*(W+(C+8)*len(sizes)+10)}")
for impl, par in [('serial_std',0)] + [('threads',p) for p in pars]:
    lbl = impl if par == 0 else f"threads(p={par})"
    row = f"  {lbl:<{W}}"
    sp_list = []
    for s in sizes:
        v = avgs.get((impl, par, s))
        r = ref.get(s)
        if v and v[0]:
            row += f"{v[0]:>{C},.1f}"
            if r and v[0] > 0 and impl != 'serial_std':
                sp = r/v[0]; sp_list.append(sp)
                row += f"{sp:>8.3f}x"
            else:
                row += f"{'1.000x' if impl=='serial_std' else '—':>8}"
        else:
            row += f"{'—':>{C}}{'—':>8}"
    avg_sp = sum(sp_list)/len(sp_list) if sp_list else None
    row += f"{avg_sp:>10.3f}x" if avg_sp else f"{'—':>10}"
    print(row)
print("=" * (W + C * len(sizes) + 10))
PYEOF
    log_ok "Summary: ${summary}"
    log_ok "Raw data: ${CSV_THREADS}"
}

print_summary_procs() {
    local summary="${RESULTS_DIR}/summary_processes.txt"
    log_section "Processes summary"
    python3 - "${CSV_PROCS}" "${CSV_THREADS}" "${CSV_SERIAL}" \
              "${PROC_MAX_N}" "${GRID_SIZES[@]}" << 'PYEOF' | tee "${summary}"
import sys, csv, math
from collections import defaultdict

p_path   = sys.argv[1]
t_path   = sys.argv[2]
s_path   = sys.argv[3]
sizes = [int(x) for x in sys.argv[5:]]

def load(path):
    rows = defaultdict(list)
    with open(path) as f:
        for r in csv.DictReader(f):
            key = (r['impl'], int(r['parallelism']), int(r['grid_size']))
            rows[key].append((float(r['wall_time_ms']), int(r['iters_done'])))
    return rows

def avg(v): return sum(v)/len(v) if v else None

all_rows = {**load(s_path), **load(t_path), **load(p_path)}
avgs = {k: (avg([d[0] for d in v]), avg([d[1] for d in v]))
        for k, v in all_rows.items()}

ref  = {s: avgs.get(('serial_std', 0, s), (None,))[0] for s in sizes}
pars = sorted({k[1] for k in load(p_path) if k[1] > 0})

W, C = 22, 18
print()
print("=" * (W + C * len(sizes) * 2 + 10))
print(f"  Processes suite  |  n ≤ {max(sizes)}  |  fork-per-iteration overhead cap")
print(f"  Correlation: threads(p) vs processes(p) at same p and n")
print(f"  Speedup = T(serial_std, n) / T(impl, n, p)")
print()

# Correlation table: one block per n
for s in sizes:
    r = ref.get(s)
    print(f"  n = {s}  |  serial_std = {r:,.1f} ms" if r else f"  n = {s}")
    print(f"  {'p':<6} {'threads ms':>{C}} {'sp':>8}  {'processes ms':>{C}} {'sp':>8}")
    print(f"  {'-'*(6+C+8+2+C+8+4)}")
    for p in pars:
        tv = avgs.get(('threads',   p, s))
        pv = avgs.get(('processes', p, s))
        t_ms = f"{tv[0]:>{C},.1f}" if tv and tv[0] else f"{'—':>{C}}"
        t_sp = f"{r/tv[0]:>8.3f}x" if tv and tv[0] and r else f"{'—':>8}"
        p_ms = f"{pv[0]:>{C},.1f}" if pv and pv[0] else f"{'—':>{C}}"
        p_sp = f"{r/pv[0]:>8.3f}x" if pv and pv[0] and r else f"{'—':>8}"
        print(f"  {p:<6} {t_ms} {t_sp}  {p_ms} {p_sp}")
    print()

print("=" * (W + C * len(sizes) * 2 + 10))
PYEOF
    log_ok "Summary: ${summary}"
    log_ok "Raw data: ${CSV_PROCS}"
}

# ---------------------------------------------------------------------------
print_banner() {
    echo -e "${BOLD}"
    echo "================================================================="
    echo "   Poisson 1D Jacobi -- Convergence-based speedup study"
    echo "   Tolerance      : 1e-6 (RMS residual)"
    echo "   Grid sizes     : ${GRID_SIZES[*]}"
    echo "   Parallel p     : ${PARALLEL_COUNTS[*]}  (same for threads & processes)"
    echo "   Max iters cap  : ${MAX_ITERS}"
    echo "   Repetitions    : ${REPETITIONS}"
    echo "   Date           : $(date '+%Y-%m-%d %H:%M:%S')"
    echo "================================================================="
    echo -e "${RESET}"
}

# ---------------------------------------------------------------------------
main() {
    local mode="${1:-all}"
    cd "${SCRIPT_DIR}"

    check_and_compile

    trap restore_system EXIT
    optimize_system
    print_banner

    case "${mode}" in
        serial)
            run_suite_serial ;;
        threads)
            if [[ ! -f "${CSV_SERIAL}" ]]; then
                log_error "data_serial.csv not found — run 'serial' suite first."
                exit 1
            fi
            run_suite_threads ;;
        procs)
            if [[ ! -f "${CSV_SERIAL}" || ! -f "${CSV_THREADS}" ]]; then
                log_error "data_serial.csv or data_threads.csv not found."
                log_error "Run 'serial' and 'threads' suites first."
                exit 1
            fi
            run_suite_procs ;;
        summary)
            [[ -f "${CSV_SERIAL}" ]]  && print_summary_serial
            [[ -f "${CSV_THREADS}" ]] && print_summary_threads
            [[ -f "${CSV_PROCS}" ]]   && print_summary_procs
            ;;
        all)
            run_suite_serial
            run_suite_threads
            run_suite_procs ;;
        *)
            log_error "Unknown mode: '${mode}'"
            log_error "Options: all | serial | threads | procs | summary"
            exit 1 ;;
    esac
}

main "$@"
