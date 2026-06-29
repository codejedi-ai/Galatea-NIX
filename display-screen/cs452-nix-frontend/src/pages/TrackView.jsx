import { useEffect, useRef, useState } from "react";

// Marklin track layout: simplified oval with 80 sensor positions (A1-E16).
// 5 modules × 16 sensors = 80 sensors total.
// We lay them out evenly around an oval track.

const W = 900, H = 500;
const CX = W / 2, CY = H / 2;
const RX = 340, RY = 180;  // oval radii

function sensorAngle(idx) {
  // idx 0-79, placed evenly around the oval (0 = top-center, clockwise)
  return (idx / 80) * 2 * Math.PI - Math.PI / 2;
}

function sensorPos(idx) {
  const a = sensorAngle(idx);
  return {
    x: CX + RX * Math.cos(a),
    y: CY + RY * Math.sin(a),
  };
}

function sensorName(idx) {
  const mod = Math.floor(idx / 16);
  const sens = (idx % 16) + 1;
  return String.fromCharCode(65 + mod) + sens;
}

// Build the oval path string
function ovalPath() {
  return `M ${CX} ${CY - RY} A ${RX} ${RY} 0 1 1 ${CX - 0.01} ${CY - RY} Z`;
}

const MODULE_COLORS = ["#ff4444", "#ff8c00", "#ffdd00", "#44ff88", "#44aaff"];

export default function TrackView({ state, trackNum }) {
  const trainIdx = state ? state.i : -1;
  const trainPos = trainIdx >= 0 ? sensorPos(trainIdx) : null;

  return (
    <div
      style={{
        position: "fixed",
        inset: "36px 0 0 0",
        background: "#0a0a0a",
        display: "flex",
        flexDirection: "column",
        alignItems: "center",
        justifyContent: "center",
      }}
    >
      <svg
        viewBox={`0 0 ${W} ${H}`}
        width="100%"
        style={{ maxWidth: W, maxHeight: "80vh" }}
      >
        {/* Track oval */}
        <ellipse
          cx={CX} cy={CY} rx={RX} ry={RY}
          fill="none" stroke="#333" strokeWidth={18} strokeLinecap="round"
        />
        <ellipse
          cx={CX} cy={CY} rx={RX} ry={RY}
          fill="none" stroke="#1a1a1a" strokeWidth={14}
        />
        {/* Rail lines */}
        <ellipse
          cx={CX} cy={CY} rx={RX - 7} ry={RY - 7}
          fill="none" stroke="#555" strokeWidth={1.5}
        />
        <ellipse
          cx={CX} cy={CY} rx={RX + 7} ry={RY + 7}
          fill="none" stroke="#555" strokeWidth={1.5}
        />

        {/* Sensor dots */}
        {Array.from({ length: 80 }, (_, i) => {
          const { x, y } = sensorPos(i);
          const mod = Math.floor(i / 16);
          const isActive = state && state.i === i;
          return (
            <g key={i}>
              <circle
                cx={x} cy={y} r={isActive ? 7 : 4}
                fill={isActive ? "#fff" : MODULE_COLORS[mod]}
                opacity={isActive ? 1 : 0.5}
              />
              {/* Label every 4th sensor or active */}
              {(i % 4 === 0 || isActive) && (
                <text
                  x={x} y={y}
                  dx={Math.cos(sensorAngle(i)) * 18}
                  dy={Math.sin(sensorAngle(i)) * 18 + 4}
                  fill={MODULE_COLORS[mod]}
                  fontSize={isActive ? 13 : 10}
                  textAnchor="middle"
                  fontFamily="Menlo, monospace"
                  opacity={isActive ? 1 : 0.6}
                >
                  {sensorName(i)}
                </text>
              )}
            </g>
          );
        })}

        {/* Train dot */}
        {trainPos && (
          <g>
            <circle
              cx={trainPos.x} cy={trainPos.y} r={14}
              fill="none" stroke="#00ff88" strokeWidth={3}
              opacity={0.5}
            />
            <circle
              cx={trainPos.x} cy={trainPos.y} r={8}
              fill="#00ff88"
            />
          </g>
        )}

        {/* Track label */}
        <text x={CX} y={CY} textAnchor="middle" fill="#333" fontSize={22}
              fontFamily="Menlo, monospace" fontWeight="bold">
          Track {trackNum}
        </text>
        {state && (
          <text x={CX} y={CY + 28} textAnchor="middle" fill="#555" fontSize={13}
                fontFamily="Menlo, monospace">
            Train {state.t} · speed {state.v} · {state.s}
          </text>
        )}
        {!state && (
          <text x={CX} y={CY + 28} textAnchor="middle" fill="#444" fontSize={13}
                fontFamily="Menlo, monospace">
            waiting for TC1 state…
          </text>
        )}
      </svg>

      {/* Module legend */}
      <div style={{
        display: "flex", gap: 16, marginTop: 8,
        fontFamily: "Menlo, monospace", fontSize: 12,
      }}>
        {["A", "B", "C", "D", "E"].map((m, i) => (
          <span key={m} style={{ color: MODULE_COLORS[i] }}>■ {m}</span>
        ))}
      </div>
    </div>
  );
}
