from flask import Flask, request, jsonify
from flask_cors import CORS
import subprocess
import json

app = Flask(__name__)
CORS(app)

@app.route("/api/solve", methods=["POST"])
def solve():
    data = request.get_json()
    if data is None:
        return jsonify({"error": "No JSON received"}), 400
    input_str = json.dumps(data)
    try:
        result = subprocess.run(
            ["./build/solver"],
            input=input_str,
            capture_output=True,
            text=True,
            timeout=30
        )
    except subprocess.TimeoutExpired:
        return jsonify({"error": "Algorithm timed out"}), 504
    if result.returncode != 0:
        return jsonify({"error": "Solver failed", "details": result.stderr}), 500
    try:
        output = json.loads(result.stdout)
    except json.JSONDecodeError:
        output = {"raw_output": result.stdout}
    return jsonify(output)

@app.route("/", methods=["GET"])
def health():
    return jsonify({"status": "alive"})

if __name__ == "__main__":
    app.run(host="0.0.0.0", port=5000)
