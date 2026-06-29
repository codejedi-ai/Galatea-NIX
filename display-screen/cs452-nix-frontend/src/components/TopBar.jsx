import { useState } from "react";

const TABS = ["Terminal"];

const styles = {
  bar: {
    position: "fixed",
    top: 0,
    left: 0,
    right: 0,
    zIndex: 100,
    background: "#111",
    borderBottom: "1px solid #333",
    display: "flex",
    alignItems: "center",
    padding: "0 8px",
    height: "36px",
    fontFamily: "Menlo, Monaco, Consolas, monospace",
    fontSize: "13px",
    userSelect: "none",
  },
  tab: (active) => ({
    padding: "0 14px",
    height: "100%",
    display: "flex",
    alignItems: "center",
    cursor: "pointer",
    color: active ? "#00ff88" : "#888",
    borderBottom: active ? "2px solid #00ff88" : "2px solid transparent",
    transition: "color 0.15s",
    whiteSpace: "nowrap",
  }),
  indicator: (online) => ({
    marginLeft: "auto",
    width: 8,
    height: 8,
    borderRadius: "50%",
    background: online ? "#00ff88" : "#444",
    marginRight: 8,
    flexShrink: 0,
  }),
  indicatorLabel: {
    color: "#555",
    fontSize: "11px",
  },
};

export default function TopBar({ activeTab, onTabChange, stateOnline }) {
  return (
    <div style={styles.bar}>
      {TABS.map((t) => (
        <div
          key={t}
          style={styles.tab(activeTab === t)}
          onClick={() => onTabChange(t)}
        >
          {t}
        </div>
      ))}
      <div style={{ flex: 1 }} />
      <div style={styles.indicator(stateOnline)} title={stateOnline ? "state-ws connected" : "state-ws offline"} />
      <span style={styles.indicatorLabel}>TC1</span>
    </div>
  );
}
