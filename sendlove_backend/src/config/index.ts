export const config = {
  // Rate limit for sending messages (per sender + box)
  rateLimit: {
    maxMessagesPerWindow: 100,       // 100 messages max (testing value)
    windowDurationMs: 24 * 60 * 60 * 1000, // 24 hours
  },
};
