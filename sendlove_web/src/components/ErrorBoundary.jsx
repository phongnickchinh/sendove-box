import React from 'react';
import { Screen, Body, Actions, Button, CircleIcon } from './ui/Screen';

/**
 * Last safety net when a page throws during render: an on-brand message with
 * two ways out, and the technical details collapsed so they can still be
 * screenshotted for a bug report.
 */
export class ErrorBoundary extends React.Component {
  constructor(props) {
    super(props);
    this.state = { hasError: false, error: null, errorInfo: null };
  }

  static getDerivedStateFromError() {
    return { hasError: true };
  }

  componentDidCatch(error, errorInfo) {
    this.setState({ error, errorInfo });
    console.error("Uncaught error:", error, errorInfo);
  }

  render() {
    if (!this.state.hasError) return this.props.children;

    const goHome = () => {
      window.location.hash = '#/dashboard';
      window.location.reload();
    };

    return (
      <Screen>
        <Body center>
          <CircleIcon size={88} bg="var(--error-bg)" color="var(--error-fill)" icon="alert" iconSize={40} sw={2.5} />
          <h1 className="sl-title">Có lỗi khi hiện trang này</h1>
          <p className="sl-body" style={{ margin: 0 }}>
            Dữ liệu của bạn không bị ảnh hưởng. Tải lại trang thường là đủ; nếu lỗi lặp lại, hãy chụp
            phần chi tiết bên dưới gửi cho người phát triển.
          </p>
          <details style={{ alignSelf: 'stretch', textAlign: 'left' }}>
            <summary className="sl-caption" style={{ cursor: 'pointer' }}>Chi tiết kỹ thuật</summary>
            <pre className="sl-caption" style={{ whiteSpace: 'pre-wrap', overflowWrap: 'anywhere', margin: '8px 0 0' }}>
              {this.state.error && this.state.error.toString()}
              {this.state.errorInfo && this.state.errorInfo.componentStack}
            </pre>
          </details>
          <Actions>
            <Button kind="pri" onClick={() => window.location.reload()}>Tải lại trang</Button>
            <Button kind="gho" onClick={goHome}>Về trang chủ</Button>
          </Actions>
        </Body>
      </Screen>
    );
  }
}
