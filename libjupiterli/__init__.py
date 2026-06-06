import redis, json
from clickhouse_driver import Client
import uuid, time
import os, socket, sys

REDIS_URL = "redis://localhost"
redis_conn = redis.from_url(REDIS_URL, decode_responses=True)

run_id = str(uuid.uuid4())
series_ids = {} # key => series_id
run_serial_num = 0

def _send_with_response(table, msg):
    ch = Client(host="localhost", port=9000, user="default", password="", database="default")
    cols = ",".join([k for k in msg.keys()])
    ch.execute(f"insert into {table}({cols}) values", [msg])
    del ch
    
def save_run_dets():
    global run_id
    host = socket.gethostname()
    pid = os.getpid()
    print(f"process {pid}@{host} starting with run_id {run_id}")
    run_label = os.environ.get("RUN_LABEL")
    if run_label is None:
        run_label = os.environ.get("RL")        
    msg = {"run_id": run_id, "created_ts": time.time(), "host": host, "pid": pid, "argv0": sys.executable, "args": " ".join(sys.argv), "run_label": run_label}
    print("<<< msg:", msg)
    msg_response = _send_with_response("runs_dets", msg)
    print(">>> msg_response:", msg_response)

def save_series_dets(series_id, run_id, key):
    msg = {"series_id": series_id, "run_id": run_id, "key": key}
    print("<<< msg:", msg)
    msg_response = _send_with_response("series_dets", msg)
    print(">>> msg_response:", msg_response)    
    
def get_series_id(key:str) -> tuple[bool, str]:
    if key in series_ids:
        return False, series_ids.get(key)    
    global run_id
    new_series_id = run_id + "---" + str(hash(key))
    series_ids[key] = new_series_id
    return True, new_series_id

def add_serial_point(key, value):
    add_ts_point(key, -1.0, value)

def add_ts_point(key, ts, value):
    is_new_key, series_id = get_series_id(key)
    if is_new_key:
        save_series_dets(series_id, run_id, key)
    global run_serial_num
    run_serial_num += 1
    #_send({"series_id": series_id, "run_serial_num": run_serial_num, "timestamp": ts, "value": float(value)})
    global redis_conn
    redis_conn.xadd("telemetry", {"data": json.dumps({"series_id": series_id, "run_serial_num": run_serial_num, "timestamp": ts, "value": float(value)})}, maxlen = 10000)
