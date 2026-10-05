#!/usr/bin/env python3
"""Read-only acceptance monitor; reports API contracts, never detection accuracy."""
import argparse
import collections
import datetime as dt
import json
import math
import pathlib
import time
import urllib.parse
import urllib.request

STATES = {'active', 'resting', 'out_of_view', 'offline', 'unknown'}


def finite(value):
    return isinstance(value, (int, float)) and not isinstance(value, bool) and math.isfinite(value)


def validate_context(context):
    if context.get('mode') not in {'live', 'replay', 'demo'}:
        raise ValueError('invalid mode')
    if context.get('health') not in {'starting', 'online', 'offline'}:
        raise ValueError('invalid health')
    if context.get('pet_state') not in STATES:
        raise ValueError('invalid pet state')
    for key in ('freshness_seconds', 'inference_ms', 'pet_confidence'):
        if not finite(context.get(key)):
            raise ValueError('invalid numeric field: ' + key)
    if context['inference_ms'] < 0 or not 0 <= context['pet_confidence'] <= 1:
        raise ValueError('invalid inference/confidence')
    for key in ('sequence', 'dropped_frames', 'cat_count', 'person_count'):
        if type(context.get(key)) is not int or context[key] < 0:
            raise ValueError('invalid counter: ' + key)
    if context['health'] == 'offline' and context['pet_state'] != 'offline':
        raise ValueError('offline camera has a non-offline pet state')
    if context['cat_count'] > 1 and context['pet_state'] in {'active', 'resting'}:
        raise ValueError('multiple cats must not produce a single-cat activity state')


def utc(value):
    return dt.datetime.strptime(value, '%Y-%m-%dT%H:%M:%SZ').replace(tzinfo=dt.timezone.utc)


def validate_timeline(timeline):
    totals = timeline.get('summary_seconds', {})
    if set(totals) != STATES or any(type(n) is not int or n < 0 for n in totals.values()):
        raise ValueError('invalid timeline totals')
    begin = utc(timeline['day'] + 'T00:00:00Z')
    cursor = begin
    measured = dict.fromkeys(STATES, 0)
    for interval in timeline['intervals']:
        state = interval['state']
        start, end = utc(interval['start']), utc(interval['end'])
        if state not in STATES or start != cursor or end <= start or end > begin + dt.timedelta(days=1):
            raise ValueError('timeline overlaps, has gaps, or crosses the requested day')
        seconds = int((end - start).total_seconds())
        if type(interval['duration_seconds']) is not int or interval['duration_seconds'] != seconds:
            raise ValueError('interval duration mismatch')
        measured[state] += seconds
        cursor = end
    if measured != totals or sum(totals.values()) > 86400:
        raise ValueError('timeline totals mismatch')


def percentile(values, ratio):
    return None if not values else sorted(values)[max(0, math.ceil(len(values) * ratio) - 1)]


class Monitor:
    def __init__(self):
        self.states = collections.Counter()
        self.modes = set()
        self.latencies = []
        self.inferences = []
        self.samples = 0
        self.timeline_checks = 0
        self.failures = collections.Counter()
        self.last_sequence = None
        self.last_dropped = None
        self.dropped_increase = 0
        self.sequence_resets = 0
        self.recovery_stage = 0
        self.recovered = False
        self.stale_online = 0

    def observe(self, context, latency):
        validate_context(context)
        self.samples += 1
        self.modes.add(context['mode'])
        self.states[context['pet_state']] += 1
        self.latencies.append(latency)
        seq, dropped = context['sequence'], context['dropped_frames']
        if self.last_sequence is not None and seq < self.last_sequence:
            self.sequence_resets += 1
        if self.last_dropped is not None:
            self.dropped_increase += max(0, dropped - self.last_dropped)
        # Repeated polling snapshots are not new inference measurements.
        if (seq > 0 and seq != self.last_sequence and context['health'] == 'online'
                and 0 <= context['freshness_seconds'] <= 5):
            self.inferences.append(context['inference_ms'])
        self.last_sequence, self.last_dropped = seq, dropped
        if context['health'] == 'online':
            if context['freshness_seconds'] < 0 or context['freshness_seconds'] > 5:
                self.stale_online += 1
            elif self.recovery_stage == 2:
                self.recovered = True
            elif self.recovery_stage == 0:
                self.recovery_stage = 1
        elif context['health'] == 'offline' and self.recovery_stage == 1:
            self.recovery_stage = 2

    def report(self, require_live=False, require_recovery=False):
        passed = (self.samples > 0 and self.timeline_checks > 0 and not self.failures
                  and (not require_live or (self.modes == {'live'} and bool(self.inferences)))
                  and (not require_recovery or self.recovered))
        return {
            'contract_checks_passed': passed, 'sample_count': self.samples,
            'timeline_checks': self.timeline_checks, 'modes': sorted(self.modes),
            'sampled_state_counts': dict(self.states), 'failures': dict(self.failures),
            'recovery_observed': self.recovered, 'sequence_resets': self.sequence_resets,
            'stale_online_samples': self.stale_online, 'dropped_frame_increase': self.dropped_increase,
            'unique_inference_samples': len(self.inferences),
            'inference_ms_p50': percentile(self.inferences, .5),
            'inference_ms_p95': percentile(self.inferences, .95),
            'api_ms_p95': percentile(self.latencies, .95),
            'scope': 'API contracts and sampled recovery only; not recognition accuracy, FPS, or device stability acceptance',
        }


def read_json(base, route):
    with urllib.request.urlopen(base + route, timeout=4) as response:
        if response.status != 200:
            raise ValueError('non-200 response')
        return json.load(response)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--url', default='http://127.0.0.1:8090')
    parser.add_argument('--seconds', type=float, default=60)
    parser.add_argument('--interval', type=float, default=1)
    parser.add_argument('--output', default='local/acceptance/report.json')
    parser.add_argument('--allow-simulation', action='store_true')
    parser.add_argument('--require-recovery', action='store_true')
    args = parser.parse_args()
    address = urllib.parse.urlsplit(args.url)
    if (address.scheme not in {'http', 'https'} or not address.hostname or address.username
            or address.password or address.query or address.fragment):
        parser.error('use an HTTP(S) base URL without credentials/query/fragment')
    if not finite(args.seconds) or not finite(args.interval) or args.seconds <= 0 or args.interval <= 0:
        parser.error('duration and interval must be positive finite values')
    base = args.url.rstrip('/')
    monitor = Monitor()
    started = time.monotonic()
    next_history = started
    while time.monotonic() - started < args.seconds:
        tick = time.monotonic()
        try:
            context = read_json(base, '/v1/context/current')
            monitor.observe(context, round((time.monotonic() - tick) * 1000, 2))
            if not args.allow_simulation and context['mode'] != 'live':
                monitor.failures['simulation_not_live'] += 1
                break
            if tick >= next_history:
                day = (context.get('observed_at') or dt.datetime.now(dt.timezone.utc).isoformat())[:10]
                timeline = read_json(base, '/v1/timeline?day=' + day)
                if timeline.get('day') != day or timeline.get('timezone') != 'UTC':
                    raise ValueError('wrong timeline date/timezone')
                validate_timeline(timeline)
                monitor.timeline_checks += 1
                next_history = tick + 5
        except (ValueError, KeyError, TypeError):
            monitor.failures['invalid_response'] += 1
        except Exception:
            # Do not print URL credentials, source IDs, or exception bodies into logs.
            monitor.failures['request_failed'] += 1
        remaining = args.seconds - (time.monotonic() - started)
        if remaining > 0:
            time.sleep(min(remaining, max(0, args.interval - (time.monotonic() - tick))))
    report = monitor.report(not args.allow_simulation, args.require_recovery)
    report['elapsed_seconds'] = round(time.monotonic() - started, 2)
    destination = pathlib.Path(args.output)
    destination.parent.mkdir(parents=True, exist_ok=True)
    destination.write_text(json.dumps(report, ensure_ascii=False, indent=2) + '\n')
    print(json.dumps(report, ensure_ascii=False, indent=2))
    return 0 if report['contract_checks_passed'] else 1


if __name__ == '__main__':
    raise SystemExit(main())
