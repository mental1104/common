package com.mental1104.flink.tests;

import static org.junit.jupiter.api.Assertions.assertEquals;
import static org.junit.jupiter.api.Assertions.assertNotNull;

import com.mental1104.flink.impl.DemoPipeline;
import org.apache.flink.streaming.api.datastream.DataStream;
import org.apache.flink.streaming.api.environment.StreamExecutionEnvironment;
import org.junit.jupiter.api.Test;

class DemoPipelineTest {
  @Test
  void buildsPipeline() {
    StreamExecutionEnvironment env = StreamExecutionEnvironment.getExecutionEnvironment();
    DataStream<Integer> stream = DemoPipeline.build(env);
    assertNotNull(stream);
  }

  @Test
  void jobNameIsStable() {
    assertEquals("Flink DataStream Demo", DemoPipeline.jobName());
  }
}
