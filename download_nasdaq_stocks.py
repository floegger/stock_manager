#!/usr/bin/env python3
"""
download_nasdaq_stocks.py

Downloads up to 1000 NASDAQ-listed stocks with 30 calendar days of historical
OHLCV data and saves each one as:

    <name>_<ticker>_<wkn>.csv

The CSV format matches Stock::importCSV() exactly:
    date,close,volume,open,high,low

WKN derivation
--------------
WKN (Wertpapierkennnummer) is a 6-character German securities ID.
For US stocks it is derived from the ISIN as the last 6 characters of the
NSIN portion (ISIN characters at index 2-7, zero-padded when shorter).
Example: ISIN US0378331005  →  WKN 037833
If the ISIN is unavailable the ticker symbol is used as a fallback.

Dependencies
------------
    pip install yfinance requests pandas

Usage
-----
    python3 download_nasdaq_stocks.py [--out-dir data] [--workers 8] [--limit 1000]
"""

import argparse
import logging
import os
import re
import sys
import time
from concurrent.futures import ThreadPoolExecutor, as_completed

import pandas as pd
import requests
import yfinance as yf

# ---------------------------------------------------------------------------
# Logging
# ---------------------------------------------------------------------------
logging.basicConfig(
    level=logging.INFO,
    format="%(asctime)s  %(levelname)-8s  %(message)s",
    datefmt="%H:%M:%S",
)
log = logging.getLogger(__name__)

# ---------------------------------------------------------------------------
# Constants
# ---------------------------------------------------------------------------
NASDAQ_SCREENER_URL = (
    "https://api.nasdaq.com/api/screener/stocks"
    "?tableonly=true&limit={limit}&exchange=NASDAQ"
)
HEADERS = {
    "User-Agent": (
        "Mozilla/5.0 (X11; Linux x86_64) AppleWebKit/537.36 "
        "(KHTML, like Gecko) Chrome/120.0.0.0 Safari/537.36"
    ),
    "Accept": "application/json",
}
HISTORY_DAYS = 30  # calendar days of history to download
RETRY_ATTEMPTS = 3  # retries per ticker on transient errors
RETRY_SLEEP = 2.0  # seconds between retries


# ---------------------------------------------------------------------------
# Helpers
# ---------------------------------------------------------------------------


def sanitize_filename(text: str) -> str:
    """Replace characters that are unsafe in filenames with underscores."""
    return re.sub(r'[\\/:*?"<>|,\s]+', "_", text).strip("_")


def isin_to_wkn(isin: str) -> str:
    """
    Derive WKN from ISIN.
    The NSIN (national securities identifier) for US ISINs occupies
    characters 2-11 (10 chars).  The WKN is traditionally the first
    6 characters of that block, keeping only alphanumeric chars.
    """
    if not isin or len(isin) < 8:
        return ""
    nsin = isin[2:12]  # 10-char NSIN
    wkn = re.sub(r"[^A-Z0-9]", "", nsin.upper())[:6]
    return wkn.zfill(6) if wkn else ""


def fetch_nasdaq_tickers(limit: int) -> list[dict]:
    """
    Fetch up to *limit* NASDAQ tickers from the NASDAQ screener API.
    Returns a list of dicts with keys: symbol, name.
    """
    url = NASDAQ_SCREENER_URL.format(limit=limit)
    log.info("Fetching ticker list from NASDAQ screener (limit=%d)…", limit)
    resp = requests.get(url, headers=HEADERS, timeout=30)
    resp.raise_for_status()
    data = resp.json()
    rows = data["data"]["table"]["rows"]
    log.info("  Received %d tickers from screener.", len(rows))
    return rows  # [{"symbol": "AAPL", "name": "Apple Inc. …", …}, …]


def download_one(
    symbol: str,
    company_name: str,
    out_dir: str,
) -> str | None:
    """
    Download 30 days of OHLCV history for *symbol*, look up its ISIN/WKN,
    and write a CSV file.  Returns the output path on success, None on failure.
    """
    for attempt in range(1, RETRY_ATTEMPTS + 1):
        try:
            ticker = yf.Ticker(symbol)

            # --- resolve WKN ---
            isin = ""
            wkn = ""
            try:
                raw_isin = ticker.isin or ""
                # yfinance returns "-" as a placeholder when ISIN is unavailable
                isin = raw_isin if (raw_isin and raw_isin != "-") else ""
                wkn = isin_to_wkn(isin)
            except Exception:
                isin = ""
            if not wkn:
                wkn = sanitize_filename(symbol)  # fallback

            # --- fetch history (30 calendar days = ~21 trading days) ---
            hist: pd.DataFrame = ticker.history(period="1mo", auto_adjust=True)
            if hist.empty:
                log.warning("[%s] No historical data returned.", symbol)
                return None

            # Keep only the last HISTORY_DAYS calendar rows (already sorted asc)
            hist = hist.tail(HISTORY_DAYS)

            # Normalise column names (yfinance may capitalise differently)
            hist.columns = [c.lower() for c in hist.columns]
            required = {"open", "high", "low", "close", "volume"}
            if not required.issubset(hist.columns):
                log.warning(
                    "[%s] Missing expected columns: %s", symbol, hist.columns.tolist()
                )
                return None

            # --- build output rows ---
            safe_name = sanitize_filename(company_name)
            safe_ticker = sanitize_filename(symbol)
            safe_wkn = sanitize_filename(wkn)
            filename = f"{safe_name}_{safe_ticker}_{safe_wkn}.csv"
            filepath = os.path.join(out_dir, filename)

            with open(filepath, "w", newline="") as fh:
                fh.write("date,close,volume,open,high,low\n")
                for ts, row in hist.iterrows():
                    date_str = ts.strftime("%Y-%m-%d")
                    fh.write(
                        f"{date_str},"
                        f"{row['close']:.4f},"
                        f"{int(row['volume'])},"
                        f"{row['open']:.4f},"
                        f"{row['high']:.4f},"
                        f"{row['low']:.4f}\n"
                    )

            log.info(
                "[%s] ✓  %s  (ISIN: %s  WKN: %s)", symbol, filename, isin or "n/a", wkn
            )
            return filepath

        except Exception as exc:
            if attempt < RETRY_ATTEMPTS:
                log.warning(
                    "[%s] attempt %d failed (%s) – retrying in %.1fs…",
                    symbol,
                    attempt,
                    exc,
                    RETRY_SLEEP,
                )
                time.sleep(RETRY_SLEEP)
            else:
                log.error(
                    "[%s] failed after %d attempts: %s", symbol, RETRY_ATTEMPTS, exc
                )
                return None


# ---------------------------------------------------------------------------
# Main
# ---------------------------------------------------------------------------


def main() -> None:
    parser = argparse.ArgumentParser(
        description="Download NASDAQ stocks with 30-day OHLCV history."
    )
    parser.add_argument(
        "--out-dir",
        default="data",
        help="Directory to write CSV files into (default: ./data).",
    )
    parser.add_argument(
        "--workers",
        type=int,
        default=8,
        help="Number of parallel download workers (default: 8).",
    )
    parser.add_argument(
        "--limit",
        type=int,
        default=1000,
        help="Maximum number of tickers to download (default: 1000).",
    )
    args = parser.parse_args()

    os.makedirs(args.out_dir, exist_ok=True)
    log.info("Output directory: %s", os.path.abspath(args.out_dir))

    # 1. Fetch ticker list
    try:
        rows = fetch_nasdaq_tickers(args.limit)
    except Exception as exc:
        log.error("Could not fetch ticker list: %s", exc)
        sys.exit(1)

    tickers = rows[: args.limit]
    total = len(tickers)
    log.info("Starting download of %d tickers with %d workers…", total, args.workers)

    # 2. Download in parallel
    success = 0
    failure = 0

    with ThreadPoolExecutor(max_workers=args.workers) as pool:
        futures = {
            pool.submit(
                download_one,
                row["symbol"],
                row.get("name", row["symbol"]),
                args.out_dir,
            ): row["symbol"]
            for row in tickers
        }
        for i, future in enumerate(as_completed(futures), start=1):
            sym = futures[future]
            result = future.result()
            if result:
                success += 1
            else:
                failure += 1
            if i % 50 == 0 or i == total:
                log.info("Progress: %d / %d  (✓ %d  ✗ %d)", i, total, success, failure)

    log.info("Done.  %d succeeded, %d failed.", success, failure)
    if failure:
        log.warning("%d tickers could not be downloaded – see warnings above.", failure)


if __name__ == "__main__":
    main()
