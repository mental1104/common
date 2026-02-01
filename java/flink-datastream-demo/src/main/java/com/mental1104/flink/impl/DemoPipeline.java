package com.mental1104.flink.impl;

import org.apache.flink.streaming.api.datastream.DataStream;
import org.apache.flink.streaming.api.environment.StreamExecutionEnvironment;

public final class DemoPipeline {
  private DemoPipeline() {
  }

  public static DataStream<Integer> build(StreamExecutionEnvironment env) {
    return env.fromElements(1);
  }

  public static String jobName() {
    return "Flink DataStream Demo";
  }
}
