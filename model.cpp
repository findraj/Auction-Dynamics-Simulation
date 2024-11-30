/**
 * @file auction.cpp
 * @brief Auction simulation with multiple bidders
 * Bidders strategies are: Agent-bidding, Ratchet-bidding, and Sniping
 *
 * @authors Marko Olešák (xolesa00), Ján Findra (xfindr01)
 */

#include <iostream>
#include <ctime>
#include <cmath>
#include "simlib.h"
#include <cstdint>
#include <vector>
using namespace std;

#define LOGGING true

const double NUMBER_OF_ITEMS = 1000;                      // Number of auction items
const double NUMBER_OF_BIDDERS = 10;                      // Number of bidders
double currentPrice = 5.0;                                // Current price of the auction
double minimalIncrement() { return currentPrice * 0.05; } // Current increment of the auction TODO
bool firstBidPlaced = false;                              // Flag if the first bid was placed for an item
const double SINGLE_ITEM_DURATION = 60.0;                 // Duration of a single auction item
double ItemEndTime = 0;                                   // End time of the current item
uint32_t itemNumber = 0;                                  // Statistics

enum BidderType
{
    AGENT,
    RATCHET,
    SNIPER,
    NONE = -1
};

int lastBidder = NONE; // Helper variable for histogram

Facility biddingFacility("Bidding process"); // Facility for bidding
Facility runningAuction("Item auction");     // Facility for running the auction
Histogram winners("Winners", -1, 1, 4);      // Histogram of winners
Queue AgentDecidedToBid("Agent decided to bid");
Queue RatchetDecidedToBid("Ratchet decided to bid");
Queue SniperDecidedToBid("Sniper decided to bid");
Process *AgentBidsProcess;
Process *RatchetBidsProcess;
Process *SniperBidsProcess;

void logSingleBid(double bidAmount)
{
    static bool header = false;
    FILE *logFile = fopen("analysis/results/auction_detailed_log.csv", "a");
    if (logFile)
    {
        if (!header)
        {
            header = true;
            fprintf(logFile, "ItemNumber,ItemTime,BidAmount,Patience\n"); // Header
        }

        double itemTime = SINGLE_ITEM_DURATION - (ItemEndTime - Time);
        fprintf(logFile, "%d,%.1f,%.2f\n", itemNumber, itemTime, bidAmount);
        fclose(logFile);
    }
}

void returnFromQueues()
{
    while (!AgentDecidedToBid.Empty())
    {
        AgentDecidedToBid.GetFirst()->Activate();
    }
    while (!RatchetDecidedToBid.Empty())
    {
        RatchetDecidedToBid.GetFirst()->Activate();
    }
    while (!SniperDecidedToBid.Empty())
    {
        SniperDecidedToBid.GetFirst()->Activate();
    }
}

// Agent-bidding strategy
// Really quickly bids higher than the current price by minimum increment
// If the current price is higher than the bidder's valuation, the bidder stops bidding
class AgentBidder : public Process
{
private:
    double valuation;
    bool isLeading = false;
    double patience = 1.0;                                     // Start with patience 1
    double lastUpdateTime = 0;                                 // Last time updatePatience was called
    const double UPDATE_INTERVAL = SINGLE_ITEM_DURATION / 100; // Minimum time interval between updates
    double roundEndTime = 0;
    double aliveTime = 0;

public:
    AgentBidder(double val, double roundEndTime) : valuation(val)
    {
        this->roundEndTime = roundEndTime;
    }

    void Behavior()
    {
        while ((currentPrice < this->valuation) && (this->patience > Exponential(0.1)) && (Time < this->roundEndTime))
        {
            // Check if enough time has passed since the last update
            if ((Time - lastUpdateTime) >= UPDATE_INTERVAL)
            {
                updatePatience();
                lastUpdateTime = Time; // Update the timestamp
            }

            Wait(std::max(this->patience, 0.2));
            aliveTime += std::max(this->patience, 0.2);

            // Agents do not engage in bidding in the early stages of the auction
            if (Time > (this->roundEndTime - (Exponential((SINGLE_ITEM_DURATION / 4) * 3))))
            {
                if ((Random() > this->patience) && ((currentPrice + minimalIncrement()) < this->valuation))
                {
                    printf("[AGENT] bidder decided to bid with alive time %.2f\n", aliveTime);
                    Wait(0.5);
                    if (Time >= this->roundEndTime)
                    {
                        Terminate();
                    }
                    AgentDecidedToBid.Insert(this);
                    // if (AgentBidsProcess->Idle())
                    // {
                    //     AgentBidsProcess->Activate();
                    // }
                    Passivate();
                }
            }
            // Stop if patience is exhausted
        }
        if (this->patience <= 0)
        {
            printf("[AGENT] bidder ran out of patience and stopped bidding.\n");
        }
        Terminate();
    }

    void updatePatience()
    {
        // Normalize time to range [0, 1] over the auction's single item duration
        double normalizedTime = (SINGLE_ITEM_DURATION - (ItemEndTime - Time)) / SINGLE_ITEM_DURATION;

        if (normalizedTime < 0.75)
        {
            this->patience = 1.0 - (Exponential(0.02) * normalizedTime);
        }
        else
        {
            // Exponential decline for the remaining 0.2 over [0.75, 1.0]
            double remainingTime = (normalizedTime - 0.75) / (1.0 - 0.75); // Normalize [0.9, 1.0] to [0, 1]
            this->patience = 0.99 - 0.2 * pow(remainingTime, 5);           // Start from 0.98 and drop exponentially
            // printf("Normalized time: %.2f, patience %.2f\n", normalizedTime, this->patience); // TODO: REMOVE
        }

        // TODO: Remove ?
        // if (!this->isLeading)
        // {
        //     this->patience += Normal(0.01, 0.005);
        //     if (this->patience < 0)
        //         this->patience = 0;
        // }
    }
};

class AgentBids : public Process
{
    void Behavior()
    {
        while (Time < ItemEndTime)
        {
            Wait(0.1);

            if (Time >= ItemEndTime)
            {
                Passivate();
            }
            if (!AgentDecidedToBid.Empty())
            {
                if (!biddingFacility.Busy())
                {
                    Seize(biddingFacility);
                    firstBidPlaced = true;
                    currentPrice += minimalIncrement();
                    if (LOGGING)
                    {
                        logSingleBid(currentPrice);
                    }
                    printf("[AGENT] bidder placed a bid at time: %.2f. New price: %.2f\n", Time, currentPrice);
                    lastBidder = AGENT;
                    returnFromQueues();
                    Release(biddingFacility);
                }
            }
        }
        Passivate();
    }
};

// Ratchet-bidding strategy
// Bids higher than the current price by minimum increment
// If the current price is higher than the bidder's valuation, the bidder stops bidding
class RatchetBidder : public Process
{
private:
    double valuation = 0;
    bool isLeading = false;
    double patience = 1.0;                                     // Start with patience 1
    double lastUpdateTime = 0;                                 // Last time updatePatience was called
    const double UPDATE_INTERVAL = SINGLE_ITEM_DURATION / 100; // Minimum time interval between updates
    double aliveTime = 0;
    double roundEndTime = 0;

public:
    RatchetBidder(double val, double roundEndTime) : valuation(val)
    {
        this->roundEndTime = roundEndTime;
        if (Random() < 0.02)
        {
            this->valuation = INFINITY;
        }
    }

    void Behavior()
    {
        // printf("[RATCHET] bidder created with valuation %.2f\n", valuation);

        while ((currentPrice < this->valuation) && (this->patience > Exponential(0.1)) && (Time < this->roundEndTime))
        {
            if ((Time - lastUpdateTime) >= UPDATE_INTERVAL)
            {
                updatePatience();
                lastUpdateTime = Time; // Update the timestamp
            }

            Wait(std::max(this->patience, 0.2));
            aliveTime += std::max(this->patience, 0.2);

            // Check if they want to bid
            if ((Random() > this->patience) && ((currentPrice + minimalIncrement()) <= valuation))
            {
                printf("[RATCHET] bidder decided to bid with alive time %.2f\n", aliveTime);
                Wait(1);
                if (Time >= this->roundEndTime)
                {
                    Terminate();
                }
                RatchetDecidedToBid.Insert(this);
                // if (RatchetBidsProcess->Idle())
                // {
                //     RatchetBidsProcess->Activate();
                // }
                Passivate();
            }
        }
        if (this->patience <= 0)
        {
            printf("[RATCHET] ran out of patience and stopped bidding.\n");
        }
        Terminate();
    }

    void updatePatience()
    {
        // Normalize time to range [0, 1] over the auction's single item duration
        double normalizedTime = (SINGLE_ITEM_DURATION - (this->roundEndTime - Time)) / SINGLE_ITEM_DURATION;

        // Smooth decay
        if (normalizedTime < 0.75)
        {

            this->patience = 1.0 - (Exponential(0.02) * normalizedTime);
        }
        else
        {
            // Exponential decline for the remaining 0.2 over [0.75, 1.0]
            double remainingTime = (normalizedTime - 0.75) / (1.0 - 0.75); // Normalize [0.9, 1.0] to [0, 1]
            this->patience = 0.99 - 0.2 * pow(remainingTime, 5);           // Start from 0.98 and drop exponentially
        }
    }
};

class RatchetBids : public Process
{
    void Behavior()
    {
        while (Time < ItemEndTime)
        {
            Wait(0.1);
            if (Time >= ItemEndTime)
            {
                Passivate();
            }
            if (!RatchetDecidedToBid.Empty())
            {
                if (!biddingFacility.Busy())
                {
                    Seize(biddingFacility);
                    firstBidPlaced = true;
                    currentPrice += minimalIncrement();
                    if (LOGGING)
                    {
                        logSingleBid(currentPrice);
                    }
                    printf("[RATCHET] bidder placed a bid at time: %.2f. New price: %.2f\n", Time, currentPrice);
                    lastBidder = RATCHET;
                    returnFromQueues();
                    Release(biddingFacility);
                }
            }
        }
        Passivate();
    }
};

// Sniping strategy
// Bids higher than the current price by minimum increment
// Sniper wait for the very last moment to bid
class SnipingBidder : public Process
{
public:
    double valuation = 0;
    double snipeDelay = Normal(0, 0.1 / 3);
    double roundEndTime = 0;

    SnipingBidder(double val, double roundEndTime) : valuation(val)
    {
        this->roundEndTime = roundEndTime;
    }

    void Behavior()
    {
        // printf("[SNIPER] bidder created with valuation %.2f\n", valuation);
        double snipeTime = this->roundEndTime - snipeDelay;
        if (Time < snipeTime)
        {
            Wait(snipeTime - Time);
        }

        Wait(Exponential(0.2)); // Reaction time
        Wait(Exponential(0.1)); // Network latency

        if (Time > this->roundEndTime)
        {
            Terminate();
        }

        if ((currentPrice + minimalIncrement()) <= valuation)
        {
            printf("[SNIPER No. %lu] bidder decided to bid at time: %.2f\n", id(), Time);
            SniperDecidedToBid.Insert(this);
            Passivate();
        }
        Terminate();
    }
};

class SniperBids : public Process
{
    void Behavior()
    {
        while (Time < ItemEndTime)
        {
            Wait(0.1);
            if (Time >= ItemEndTime)
            {
                Passivate();
            }
            if (!SniperDecidedToBid.Empty())
            {
                if (!biddingFacility.Busy())
                {
                    Seize(biddingFacility);
                    firstBidPlaced = true;
                    currentPrice += minimalIncrement();
                    if (LOGGING)
                    {
                        logSingleBid(currentPrice);
                    }
                    printf("[SNIPER No. %lu] bidder placed a bid at time: %.2f. New price: %.2f\n", SniperDecidedToBid.GetFirst()->id(), Time, currentPrice);
                    lastBidder = SNIPER;
                    returnFromQueues();
                    Release(biddingFacility);
                }
            }
        }
        Passivate();
    }
};

class BidderGenerator : public Event
{
private:
    double RoundEndTime = 0;
    double RealPrice = 0;

public:
    BidderGenerator(double roundEndTime, double realPrice)
    {
        this->RoundEndTime = roundEndTime;
        this->RealPrice = realPrice;
    }

    void Behavior()
    {
        // Generate bidders
        int agents = 0;
        int ratchets = 0;
        int snipers = 0;
        for (int i = 0; i < NUMBER_OF_BIDDERS; i++)
        {
            // Calculate probability of each strategy
            // Agent-bidding: 40%
            // Ratchet-bidding: 25%
            // Sniping: 35%
            // Follows the reference paper
            double probability = Random();

            // Generate bidder with the given strategy
            if (probability < 0.4)
            {
                Process *agenProc = new AgentBidder(RealPrice * Normal(1.2, 0.5 / 2), this->RoundEndTime); // TODO: V dokumentacii povedat, ze nastavene podla toho, ze ebay v priemere 16 bids na aukciu
                agenProc->Activate();
                agents++;
            }
            else if (probability < 0.65)
            {
                Process *ratchetProc = new RatchetBidder(RealPrice * Normal(1.2, 0.5 / 2), this->RoundEndTime);
                ratchetProc->Activate();
                ratchets++;
            }
            else
            {
                // Snipers generally do not want to bid, when the price is high
                Process *sniperProc = new SnipingBidder(RealPrice * Normal(1.2, 0.3 / 2), this->RoundEndTime);
                sniperProc->Activate();
                snipers++;
            }
        }
        printf("Generated %d agents, %d ratchets, %d snipers\n", agents, ratchets, snipers);
    }
};

class FirstBidTimeout : public Event
{
    Process *id;
    bool *placed;

public:
    FirstBidTimeout(Process *p, double dt, bool *firstBidPlaced) : id(p)
    {
        placed = firstBidPlaced;
        Activate(Time + dt);
    }

    void Behavior()
    {
        if (!*placed)
        {
            printf("No bids were placed in the first 30 seconds, the item is discarded\n");
            // id->Release(runningAuction);
            id->Cancel();
            winners(NONE);
        }
        Cancel();
    }
};

// Represents one auction item
class AuctionItem : public Process
{
public:
    bool isSold = false;
    void Behavior()
    {
        Priority = 10;
        // Generate bidders
        ItemEndTime = Time + SINGLE_ITEM_DURATION;

        itemNumber++;

        // Generate the value of the item
        double RealPrice = Exponential(1000 * Normal(1.0, 0.2));
        printf("Created item with value %.2f\n", RealPrice);

        lastBidder = NONE;
        printf("Resetting last bidder to %d\n", lastBidder);

        currentPrice = RealPrice * Normal(0.8, 0.2);

        // Reset the current price
        printf("Auction started for item valued at %.2f\n", currentPrice);

        AgentBidsProcess = new AgentBids();
        RatchetBidsProcess = new RatchetBids();
        SniperBidsProcess = new SniperBids();
        AgentBidsProcess->Activate();
        RatchetBidsProcess->Activate();
        SniperBidsProcess->Activate();

        // Create bidders
        (new BidderGenerator(ItemEndTime, RealPrice))->Activate();

        // If there are no bidders in the first 30 seconds, the item is discarded
        FirstBidTimeout *firstBidTimeout = new FirstBidTimeout(this, 30, &firstBidPlaced); // TODO: Pomocna fronta miesto boolu

        printf("This auction will end at %.2f\n", ItemEndTime);
        printf("Current time is %.2f\n", Time);
        // Wait until the end of the auction
        Wait(SINGLE_ITEM_DURATION);
        printf("Auction ended\n");

        // If a bid was placed, the item is sold
        if (firstBidPlaced)
        {
            isSold = true;
            printf("Item sold at price %.2f\n", currentPrice);
            printf("Winner: %d\n", lastBidder);
            winners(lastBidder);
        }
        else
        {
            // Should not happen, it is caught by the timeout
            printf("Item not sold (no bids)\n");
        }

        // Terminate the bids processes
        AgentBidsProcess->Terminate();
        RatchetBidsProcess->Terminate();
        SniperBidsProcess->Terminate();
        delete firstBidTimeout;
        Terminate();
    }
};

class Auction : public Process
{
public:
    void Behavior()
    {
        while (itemNumber < NUMBER_OF_ITEMS)
        {
            Seize(runningAuction); // indicated the end of the auction for a single item
            printf("AUCTION STARTED\n");

            // Create and activate an auction item
            AuctionItem *item = new AuctionItem();
            item->Activate();

            returnFromQueues();
            // Pause between items
            Wait(60);

            Release(runningAuction);
        }
        printf("All items sold for today\n");
    }
};

int main()
{
    RandomSeed(time(NULL));
    Init(0, (SINGLE_ITEM_DURATION * 2 + 10) * NUMBER_OF_ITEMS);
    (new Auction)->Activate();
    Run();

    // Statistics
    SetOutput("stats.out");
    printf("Simulation finished\n");
    biddingFacility.Output();
    winners.Output();
    runningAuction.Output();
}