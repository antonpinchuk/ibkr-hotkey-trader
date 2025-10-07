#!/usr/bin/env python3
"""
Simple test script to verify TWS API position data
"""

from ibapi.client import EClient
from ibapi.wrapper import EWrapper
from ibapi.contract import Contract
from ibapi.common import TickerId
import threading
import time

class TestApp(EWrapper, EClient):
    def __init__(self):
        EWrapper.__init__(self)
        EClient.__init__(self, self)
        self.positions = []
        self.positions_received = False

    def error(self, reqId: TickerId, errorTime, errorCode: int, errorString: str, advancedOrderRejectJson: str = ""):
        print(f"ERROR: reqId={reqId}, code={errorCode}, msg={errorString}")

    def nextValidId(self, orderId: int):
        print(f"Connected! Next valid order ID: {orderId}")
        print("\n=== Requesting positions ===\n")
        self.reqPositions()

    def position(self, account: str, contract: Contract, position, avgCost: float):
        print(f"\n=== POSITION CALLBACK ===")
        print(f"  Account: {account}")
        print(f"  Symbol: {contract.symbol}")
        print(f"  SecType: {contract.secType}")
        print(f"  Exchange: {contract.exchange}")
        print(f"  Currency: {contract.currency}")
        print(f"  Position (Decimal): {position}")
        print(f"  Position (type): {type(position)}")
        print(f"  Position (str): {str(position)}")
        print(f"  Position (repr): {repr(position)}")
        print(f"  Avg Cost: {avgCost}")

        # Try to convert to float
        try:
            pos_float = float(position)
            print(f"  Position (float): {pos_float}")
        except Exception as e:
            print(f"  Position (float conversion failed): {e}")

        self.positions.append({
            'account': account,
            'symbol': contract.symbol,
            'position': position,
            'avgCost': avgCost
        })

    def positionEnd(self):
        print("\n=== POSITIONS END ===")
        print(f"Total positions received: {len(self.positions)}")
        self.positions_received = True

    def updatePortfolio(self, contract: Contract, position,
                        marketPrice: float, marketValue: float,
                        averageCost: float, unrealizedPNL: float,
                        realizedPNL: float, accountName: str):
        print(f"\n=== UPDATE PORTFOLIO CALLBACK ===")
        print(f"  Account: {accountName}")
        print(f"  Symbol: {contract.symbol}")
        print(f"  Position (Decimal): {position}")
        print(f"  Position (type): {type(position)}")
        print(f"  Position (str): {str(position)}")
        print(f"  Position (repr): {repr(position)}")
        print(f"  Market Price: {marketPrice}")
        print(f"  Market Value: {marketValue}")
        print(f"  Avg Cost: {averageCost}")
        print(f"  Unrealized PNL: {unrealizedPNL}")
        print(f"  Realized PNL: {realizedPNL}")

        # Try to convert to float
        try:
            pos_float = float(position)
            print(f"  Position (float): {pos_float}")
        except Exception as e:
            print(f"  Position (float conversion failed): {e}")

    def accountDownloadEnd(self, accountName: str):
        print(f"\n=== ACCOUNT DOWNLOAD END: {accountName} ===")

def main():
    print("TWS API Position Test")
    print("=" * 50)

    app = TestApp()

    # Connect to TWS (default: localhost:7496 for TWS, 4001 for IB Gateway)
    host = "127.0.0.1"
    port = 7496  # Change to 4001 if using IB Gateway
    clientId = 999  # Use unique client ID

    print(f"Connecting to TWS at {host}:{port} (clientId={clientId})...")
    app.connect(host, port, clientId)

    # Start the socket in a thread
    api_thread = threading.Thread(target=app.run, daemon=True)
    api_thread.start()

    # Wait for positions to be received
    timeout = 15
    start = time.time()
    while not app.positions_received and (time.time() - start) < timeout:
        time.sleep(0.1)

    if app.positions_received:
        print("\n" + "=" * 50)
        print("SUMMARY")
        print("=" * 50)
        for pos in app.positions:
            print(f"{pos['symbol']:10s} | Position: {pos['position']} | Avg Cost: ${pos['avgCost']:.2f}")
    else:
        print(f"\nTimeout after {timeout}s waiting for positions")

    print("\nDisconnecting...")
    app.disconnect()
    time.sleep(1)
    print("Done!")

if __name__ == "__main__":
    main()
