Rack::~Rack()
{
    // Host must stop audio processing before Rack destruction.

    drainRetired();

    Command command;

    while (tryPopCommand(command))
    {
        if (command.type == CommandType::replaceChain)
        {
            delete command.chain;
            command.chain = nullptr;
        }
    }

    delete deferredNewChain;
    deferredNewChain = nullptr;

    // deferredRetiredChain can still be the active chain.
    if (deferredRetiredChain == activeChain)
        activeChain = nullptr;

    delete deferredRetiredChain;
    deferredRetiredChain = nullptr;

    delete activeChain;
    activeChain = nullptr;

    drainRetired();
}