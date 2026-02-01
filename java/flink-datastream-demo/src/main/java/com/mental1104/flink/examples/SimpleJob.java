package com.mental1104.flink.examples;

import org.apache.flink.streaming.api.environment.StreamExecutionEnvironment;
import org.apache.flink.streaming.api.datastream.DataStream;
import com.mental1104.flink.impl.DemoPipeline;

public final class SimpleJob {
  private SimpleJob() {
  }

  public static void main(String[] args) throws Exception {
    StreamExecutionEnvironment env = StreamExecutionEnvironment.getExecutionEnvironment();
    env.setParallelism(1);

    DataStream<Integer> identity = DemoPipeline.build(env);

    identity.print();
    env.execute(DemoPipeline.jobName());
  }
}
