"""Formato de salida (texto legible y CSV)."""

from dataclasses import dataclass

LANGUAGE = "Python"
CSV_HEADER = "language,mode,workers,workload,time_seconds,result"


@dataclass(frozen=True)
class RunRecord:
    mode: str
    workers: int
    workload: int
    seconds: float
    result: int


def format_text(record: RunRecord) -> str:
    return "\n".join(
        [
            f"Language: {LANGUAGE}",
            f"Mode: {record.mode}",
            f"Workers: {record.workers}",
            f"Workload: {record.workload}",
            f"Execution time: {record.seconds:.4f} seconds",
            f"Result: {record.result}",
        ]
    )


def format_csv(record: RunRecord) -> str:
    return (
        f"{LANGUAGE},{record.mode},{record.workers},{record.workload},"
        f"{record.seconds:.6f},{record.result}"
    )
