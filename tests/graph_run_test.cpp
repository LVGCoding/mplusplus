// Graphs from a run: the results file the program writes, read back by the
// editor (run/Results.h), and documents with graphs end to end.

#include "editor/FileMpp.h"
#include "run/Driver.h"
#include "run/Results.h"
#include "run/Run.h"
#include "run/Toolchain.h"

#include <gtest/gtest.h>

#include <cmath>
#include <string>
#include <vector>

TEST(Results, ReadsAPlot) {
    const auto plot = run::ParsePlot(
        R"({"kind":"graph","id":"node:7","title":"Growth \"fast\"","xlabel":"t","ylabel":"",)"
        R"("xscale":"log","yscale":"linear","xmin":1,"xmax":100,"series":[)"
        R"({"type":"line","x":[1,2,null],"y":[3,4,5],"legend":"y = x^2","color":"",)"
        R"("shape":"","width":0},{"type":"bar","x":[1],"y":[2],"legend":"","color":"red",)"
        R"("shape":"","width":0.8,"labels":["a"]}]})");
    ASSERT_TRUE(plot);
    EXPECT_EQ(plot->id, "node:7");
    EXPECT_EQ(plot->title, "Growth \"fast\"");
    EXPECT_EQ(plot->xscale, "log");
    EXPECT_EQ(plot->xmax, 100);
    EXPECT_FALSE(plot->ymin);
    ASSERT_EQ(plot->series.size(), 2u);
    EXPECT_EQ(plot->series[0].legend, "y = x^2");
    EXPECT_TRUE(std::isnan(plot->series[0].x[2]));
    EXPECT_EQ(plot->series[1].labels, std::vector<std::string>{"a"});
    EXPECT_DOUBLE_EQ(plot->series[1].width, 0.8);
}

TEST(Results, KeepsTheLastVersionOfEachPlot) {
    const auto plots = run::LatestPlots({
        R"({"kind":"graph","id":"figure:1","title":"a","series":[]})",
        "not json",
        R"({"kind":"graph","id":"node:2","title":"b","series":[]})",
        R"({"kind":"graph","id":"figure:1","title":"c","series":[]})",
    });
    ASSERT_EQ(plots.size(), 2u);
    EXPECT_EQ(plots[0].title, "c");
    EXPECT_EQ(plots[1].title, "b");
}

namespace {

// Compile and run a document; the plots it sends back.
std::vector<run::PlotData> RunAndCollect(const std::string& source, std::string& output) {
    FileMpp file;
    file.SetSource(source);
    Translation translation = TranslateFile(file);
    for (const auto& diagnostic : translation.diagnostics) {
        output += "compile error: " + diagnostic.message + "\n";
    }
    if (!translation.succeeded) {
        return {};
    }
    run::Job job;
    job.Start(translation.cpp);
    job.Wait();
    for (const auto& line : job.TakeOutput()) {
        if (line.kind != run::OutputLine::Kind::Info) {
            output += line.text + "\n";
        }
    }
    return run::LatestPlots(job.TakeResults());
}

}  // namespace

TEST(GraphRun, AGraphInTheDocumentSendsItsFunctionsAndData) {
    std::string error;
    if (!run::FindToolchain(error)) {
        GTEST_SKIP() << error;
    }
    std::string output;
    const auto plots = RunAndCollect(
        "a := 2\n"
        "xs := [1, 2, 3]\n"
        "ys := [1, 4, 9]\n"
        "\\begin{graph}[title=Parabolas, xmin=-1, xmax=1] y=a \\cdot x\\exp{}{2} \\\\ \\sqrt{x} \\\\ "
        "(xs, ys) \\end{graph}\n",
        output);
    EXPECT_EQ(output, "");
    ASSERT_EQ(plots.size(), 1u);
    const run::PlotData& plot = plots[0];
    EXPECT_EQ(plot.id.rfind("node:", 0), 0u);
    EXPECT_EQ(plot.title, "Parabolas");
    ASSERT_EQ(plot.series.size(), 3u);
    // y = a x² sampled from -1 to 1, using a from above the graph.
    const auto& parabola = plot.series[0];
    EXPECT_EQ(parabola.type, "line");
    EXPECT_EQ(parabola.legend, "y=a ⋅x^2");
    ASSERT_EQ(parabola.x.size(), 400u);
    EXPECT_DOUBLE_EQ(parabola.x.front(), -1);
    EXPECT_DOUBLE_EQ(parabola.y.front(), 2);
    EXPECT_DOUBLE_EQ(parabola.y.back(), 2);
    // √x has no value left of 0: a gap, not an error.
    EXPECT_TRUE(std::isnan(plot.series[1].y.front()));
    EXPECT_DOUBLE_EQ(plot.series[1].y.back(), 1);
    // The data as points.
    EXPECT_EQ(plot.series[2].type, "scatter");
    EXPECT_EQ(plot.series[2].y, (std::vector<double>{1, 4, 9}));
}

TEST(GraphRun, AFigureSendsItsSeries) {
    std::string error;
    if (!run::FindToolchain(error)) {
        GTEST_SKIP() << error;
    }
    std::string output;
    const auto plots = RunAndCollect(
        "fig := Figure<title = \"Data\", scaleY = \"log\">\n"
        "fig.Line<x = [1, 2, 3], y = [10, 100, 1000], legend = \"growth\">\n"
        "fig.Histogram<values = [1, 2, 2, 3], bins = 3>\n"
        "fig.Bar<labels = [\"a\", \"b\"], values = [3, 4], color = \"red\">\n"
        "print(\"done\")\n",
        output);
    EXPECT_EQ(output, "done\n");
    ASSERT_EQ(plots.size(), 1u);
    EXPECT_EQ(plots[0].title, "Data");
    EXPECT_EQ(plots[0].yscale, "log");
    ASSERT_EQ(plots[0].series.size(), 3u);
    EXPECT_EQ(plots[0].series[0].legend, "growth");
    EXPECT_EQ(plots[0].series[1].y, (std::vector<double>{1, 2, 1}));
    EXPECT_EQ(plots[0].series[2].labels, (std::vector<std::string>{"a", "b"}));
}

TEST(GraphRun, MismatchedDataStopsWithAnError) {
    std::string error;
    if (!run::FindToolchain(error)) {
        GTEST_SKIP() << error;
    }
    std::string output;
    RunAndCollect("fig := Figure()\nfig.Line<x = [1, 2], y = [1]>\n", output);
    EXPECT_NE(output.find("the data has 2 x values but 1 y values"), std::string::npos) << output;
}
