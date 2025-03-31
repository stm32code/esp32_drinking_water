package com.example.intelligentdrinkingwatersystem.bean;

public class Send {
    private int cmd;
    private Integer pump;
    private Integer state;

    @Override
    public String toString() {
        return "Send{" +
                "cmd=" + cmd +
                ", pump=" + pump +
                ", state=" + state +
                '}';
    }

    public int getCmd() {
        return cmd;
    }

    public void setCmd(int cmd) {
        this.cmd = cmd;
    }

    public Integer getPump() {
        return pump;
    }

    public void setPump(Integer pump) {
        this.pump = pump;
    }

    public Integer getState() {
        return state;
    }

    public void setState(Integer state) {
        this.state = state;
    }
}
