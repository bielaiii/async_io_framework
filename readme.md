### target

- [x] 支持异步connect
- [x] 支持异步accept
- [x] 支持异步write
- [x] 支持异步read
- [x] 支持定时器功能
- [ ] 支持read_at, read_untile(因为asio有)
- [x] 支持读一些内容（不要求读满buffer）
- [ ] 支持取消异步读写
- [ ] 惰性求值
- [ ] 未来支持std::execution

### 原则

零成本抽象

## you dont't pay for what you don't use