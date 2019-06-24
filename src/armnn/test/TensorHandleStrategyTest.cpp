//
// Copyright © 2017 Arm Ltd. All rights reserved.
// SPDX-License-Identifier: MIT
//
#include <boost/test/unit_test.hpp>

#include <armnn/LayerVisitorBase.hpp>

#include <backendsCommon/IBackendContext.hpp>
#include <backendsCommon/IBackendInternal.hpp>
#include <backendsCommon/IMemoryManager.hpp>
#include <backendsCommon/ITensorHandleFactory.hpp>
#include <backendsCommon/TensorHandleFactoryRegistry.hpp>

#include <optimizations/Optimization.hpp>

#include <Network.hpp>

#include <vector>
#include <string>

using namespace armnn;

class TestMemMgr : public IMemoryManager
{
public:
    TestMemMgr() = default;

    void Acquire() override {}
    void Release() override {}
};

class TestFactory1 : public ITensorHandleFactory
{
public:
    TestFactory1(std::weak_ptr<IMemoryManager> mgr, ITensorHandleFactory::FactoryId id)
        : m_Id(id)
        , m_MemMgr(mgr)
    {}

    std::unique_ptr<ITensorHandle> CreateSubTensorHandle(ITensorHandle& parent,
                                                         TensorShape const& subTensorShape,
                                                         unsigned int const* subTensorOrigin) const override
    {
        return nullptr;
    }

    std::unique_ptr<ITensorHandle> CreateTensorHandle(const TensorInfo& tensorInfo) const override
    {
        return nullptr;
    }

    virtual const FactoryId GetId() const override { return m_Id; }

    virtual bool SupportsSubTensors() const override { return true; }

private:
    FactoryId m_Id = "UninitializedId";

    std::weak_ptr<IMemoryManager> m_MemMgr;
};

class TestBackendA : public IBackendInternal
{
public:
    TestBackendA() = default;

    const BackendId& GetId() const override { return m_Id; }

    IWorkloadFactoryPtr CreateWorkloadFactory(const IMemoryManagerSharedPtr& memoryManager = nullptr) const override
    {
        return IWorkloadFactoryPtr{};
    }

    IBackendInternal::ILayerSupportSharedPtr GetLayerSupport() const override
    {
        return ILayerSupportSharedPtr{};
    }

    std::vector<ITensorHandleFactory::FactoryId> GetHandleFactoryPreferences() const override
    {
        return std::vector<ITensorHandleFactory::FactoryId>
        {
            "TestHandleFactoryA1",
            "TestHandleFactoryA2",
            "TestHandleFactoryB1"
        };
    }

    void RegisterTensorHandleFactories(TensorHandleFactoryRegistry& registry) override
    {
        auto mgr = std::make_shared<TestMemMgr>();

        registry.RegisterMemoryManager(mgr);
        registry.RegisterFactory(std::make_unique<TestFactory1>(mgr, "TestHandleFactoryA1"));
        registry.RegisterFactory(std::make_unique<TestFactory1>(mgr, "TestHandleFactoryA2"));
    }

private:
    BackendId m_Id = "BackendA";
};

class TestBackendB : public IBackendInternal
{
public:
    TestBackendB() = default;

    const BackendId& GetId() const override { return m_Id; }

    IWorkloadFactoryPtr CreateWorkloadFactory(const IMemoryManagerSharedPtr& memoryManager = nullptr) const override
    {
        return IWorkloadFactoryPtr{};
    }

    IBackendInternal::ILayerSupportSharedPtr GetLayerSupport() const override
    {
        return ILayerSupportSharedPtr{};
    }

    std::vector<ITensorHandleFactory::FactoryId> GetHandleFactoryPreferences() const override
    {
        return std::vector<ITensorHandleFactory::FactoryId>
        {
            "TestHandleFactoryB1"
        };
    }

    void RegisterTensorHandleFactories(TensorHandleFactoryRegistry& registry) override
    {
        auto mgr = std::make_shared<TestMemMgr>();

        registry.RegisterMemoryManager(mgr);
        registry.RegisterFactory(std::make_unique<TestFactory1>(mgr, "TestHandleFactoryB1"));
    }

private:
    BackendId m_Id = "BackendB";
};

class TestBackendC : public IBackendInternal
{
public:
    TestBackendC() = default;

    const BackendId& GetId() const override { return m_Id; }

    IWorkloadFactoryPtr CreateWorkloadFactory(const IMemoryManagerSharedPtr& memoryManager = nullptr) const override
    {
        return IWorkloadFactoryPtr{};
    }

    IBackendInternal::ILayerSupportSharedPtr GetLayerSupport() const override
    {
        return ILayerSupportSharedPtr{};
    }

    std::vector<ITensorHandleFactory::FactoryId> GetHandleFactoryPreferences() const override
    {
        return std::vector<ITensorHandleFactory::FactoryId>{
            "TestHandleFactoryC1"
        };
    }

    void RegisterTensorHandleFactories(TensorHandleFactoryRegistry& registry) override
    {
        auto mgr = std::make_shared<TestMemMgr>();

        registry.RegisterMemoryManager(mgr);
        registry.RegisterFactory(std::make_unique<TestFactory1>(mgr, "TestHandleFactoryC1"));
    }

private:
    BackendId m_Id = "BackendC";
};


BOOST_AUTO_TEST_SUITE(TensorHandle)

BOOST_AUTO_TEST_CASE(RegisterFactories)
{
    TestBackendA backendA;
    TestBackendB backendB;

    BOOST_TEST(backendA.GetHandleFactoryPreferences()[0] == "TestHandleFactoryA1");
    BOOST_TEST(backendA.GetHandleFactoryPreferences()[1] == "TestHandleFactoryA2");
    BOOST_TEST(backendA.GetHandleFactoryPreferences()[2] == "TestHandleFactoryB1");

    TensorHandleFactoryRegistry registry;
    backendA.RegisterTensorHandleFactories(registry);
    backendB.RegisterTensorHandleFactories(registry);

    BOOST_TEST((registry.GetFactory("Non-existing Backend") == nullptr));
    BOOST_TEST((registry.GetFactory("TestHandleFactoryA1") != nullptr));
    BOOST_TEST((registry.GetFactory("TestHandleFactoryA2") != nullptr));
    BOOST_TEST((registry.GetFactory("TestHandleFactoryB1") != nullptr));
}

BOOST_AUTO_TEST_CASE(TensorHandleSelectionStrategy)
{
    auto backendA = std::make_unique<TestBackendA>();
    auto backendB = std::make_unique<TestBackendB>();
    auto backendC = std::make_unique<TestBackendC>();

    TensorHandleFactoryRegistry registry;
    backendA->RegisterTensorHandleFactories(registry);
    backendB->RegisterTensorHandleFactories(registry);
    backendC->RegisterTensorHandleFactories(registry);

    BackendsMap backends;
    backends["BackendA"] = std::move(backendA);
    backends["BackendB"] = std::move(backendB);
    backends["BackendC"] = std::move(backendC);

    armnn::Graph graph;

    armnn::InputLayer* const inputLayer = graph.AddLayer<armnn::InputLayer>(0, "input");
    inputLayer->SetBackendId("BackendA");

    armnn::SoftmaxDescriptor smDesc;
    armnn::SoftmaxLayer* const softmaxLayer1 = graph.AddLayer<armnn::SoftmaxLayer>(smDesc, "softmax1");
    softmaxLayer1->SetBackendId("BackendA");

    armnn::SoftmaxLayer* const softmaxLayer2 = graph.AddLayer<armnn::SoftmaxLayer>(smDesc, "softmax2");
    softmaxLayer2->SetBackendId("BackendB");

    armnn::SoftmaxLayer* const softmaxLayer3 = graph.AddLayer<armnn::SoftmaxLayer>(smDesc, "softmax3");
    softmaxLayer3->SetBackendId("BackendC");

    armnn::OutputLayer* const outputLayer = graph.AddLayer<armnn::OutputLayer>(0, "output");
    outputLayer->SetBackendId("BackendA");

    inputLayer->GetOutputSlot(0).Connect(softmaxLayer1->GetInputSlot(0));
    softmaxLayer1->GetOutputSlot(0).Connect(softmaxLayer2->GetInputSlot(0));
    softmaxLayer2->GetOutputSlot(0).Connect(softmaxLayer3->GetInputSlot(0));
    softmaxLayer3->GetOutputSlot(0).Connect(outputLayer->GetInputSlot(0));

    graph.TopologicalSort();

    std::vector<std::string> errors;
    auto result = SelectTensorHandleStrategy(graph, backends, registry, errors);

    BOOST_TEST(result.m_Error == false);
    BOOST_TEST(result.m_Warning == false);

    OutputSlot& inputLayerOut = inputLayer->GetOutputSlot(0);
    OutputSlot& softmaxLayer1Out = softmaxLayer1->GetOutputSlot(0);
    OutputSlot& softmaxLayer2Out = softmaxLayer2->GetOutputSlot(0);
    OutputSlot& softmaxLayer3Out = softmaxLayer3->GetOutputSlot(0);

    // Check that the correct factory was selected
    BOOST_TEST(inputLayerOut.GetTensorHandleFactoryId() == "TestHandleFactoryA1");
    BOOST_TEST(softmaxLayer1Out.GetTensorHandleFactoryId() == "TestHandleFactoryB1");
    BOOST_TEST(softmaxLayer2Out.GetTensorHandleFactoryId() == "TestHandleFactoryB1");
    BOOST_TEST(softmaxLayer3Out.GetTensorHandleFactoryId() == "TestHandleFactoryC1");

    // Check that the correct strategy was selected
    BOOST_TEST((inputLayerOut.GetMemoryStrategyForConnection(0) == MemoryStrategy::DirectCompatibility));
    BOOST_TEST((softmaxLayer1Out.GetMemoryStrategyForConnection(0) == MemoryStrategy::DirectCompatibility));
    BOOST_TEST((softmaxLayer2Out.GetMemoryStrategyForConnection(0) == MemoryStrategy::CopyToTarget));
    BOOST_TEST((softmaxLayer3Out.GetMemoryStrategyForConnection(0) == MemoryStrategy::DirectCompatibility));

    graph.AddCopyLayers(backends, registry);
    int count= 0;
    graph.ForEachLayer([&count](Layer* layer)
    {
        if (layer->GetType() == LayerType::MemCopy)
        {
            count++;
        }
    });
    BOOST_TEST(count == 1);
}

BOOST_AUTO_TEST_SUITE_END()