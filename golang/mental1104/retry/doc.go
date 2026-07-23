// Package retry 提供受 context 总体期限约束的指数退避重试。
//
// 它支持最大尝试次数、最大退避、随机抖动和可插拔的错误分类策略。
// Sleep、Now 与 Random 可注入，便于在单元测试中稳定验证时间相关行为。
package retry
