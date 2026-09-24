let meta = #{
    name: "seraph-build-test",
    description: "Build Seraph, prove ctest, and push master when the summary is green",
    phases: [
        #{ title: "Build", detail: "configure, build, ctest" },
        #{ title: "Fix", detail: "repair failures up to the round cap" },
        #{ title: "Prove", detail: "re-run ctest and quote the summary" },
        #{ title: "Ship", detail: "commit, push, read CI" },
    ],
};

let report_schema = #{
    "type": "object",
    "required": ["green", "summary"],
    "properties": #{
        "green": #{ "type": "boolean" },
        "summary": #{ "type": "string" },
    },
};

let verdict_schema = #{
    "type": "object",
    "required": ["real", "summary"],
    "properties": #{
        "real": #{ "type": "boolean" },
        "summary": #{ "type": "string" },
    },
};

let root = "C:\\Users\\Loki\\seraph";
let cmake = "C:\\Program Files\\Microsoft Visual Studio\\2022\\Professional\\Common7\\IDE\\CommonExtensions\\Microsoft\\CMake\\CMake\\bin\\cmake.exe";
let do_push = true;
let max_rounds = 3;
if args != () {
    if args.push != () {
        do_push = args.push;
    }
    if args.max_rounds != () {
        max_rounds = args.max_rounds;
    }
}

let build_prompt = "Work only in " + root + ". Use this cmake if `cmake` is not on PATH: " + cmake + ". ctest.exe is in that same directory. Configure " + root + "\\build with -DZYDIS_BUILD_TESTS=ON -DZYAN_DEV_MODE=ON if the build directory is missing, then build the Release config and run ctest -C Release --output-on-failure. Set NO_COLOR=1 so ZydisInfo golden files are compared without VT100 color. Set green=true only when ctest exits 0. summary must be the ctest summary line you actually saw, including the tests-passed count. Do not commit or push.";

let round = 0;
let green = false;
let last_summary = "";
while round < max_rounds && green == false {
    phase("Build");
    let built = agent(build_prompt, #{
        label: "build",
        capability_mode: "execute",
        output_schema: report_schema,
    });
    if built != () && built.success && built.output.green == true && built.output.summary != () && built.output.summary != "" {
        green = true;
        last_summary = built.output.summary;
    } else {
        phase("Fix");
        let reason = "ctest failed";
        if built != () && built.success && built.output.summary != () {
            reason = built.output.summary;
        }
        agent("ctest in " + root + " failed. Reason: " + reason + ". Read the ctest output, fix the Seraph source under " + root + ", rebuild Release, and re-run ctest. Do not commit or push.", #{
            label: "fix",
            capability_mode: "execute",
        });
    }
    round += 1;
}

if green == false {
    await_user("verification", "Seraph ctest was still red after the round cap. Fix the build, then resume.");
}

phase("Prove");
let proved = agent("Re-run ctest yourself in " + root + "\\build with ctest -C Release --output-on-failure. Use " + cmake + " 's sibling ctest.exe when ctest is not on PATH. Set real=true only if you saw ctest exit 0. summary must quote that summary line verbatim. If ctest fails, set real=false.", #{
    label: "prove",
    capability_mode: "execute",
    output_schema: verdict_schema,
});

if proved == () || proved.success == false || proved.output.real != true || proved.output.summary == () || proved.output.summary == "" {
    await_user("verification", "The prove step did not quote a green ctest summary.");
}

if do_push == false {
    complete(#{ summary: proved.output.summary, pushed: false });
}

phase("Ship");
let shipped = agent("Repo " + root + " remote origin is https://github.com/kuro1337WStuff/seraph.git branch master. ctest summary just proved green: " + proved.output.summary + ". If the worktree has changes, commit them with the repo git identity. Do not add a Co-Authored-By trailer or any agent attribution. Do not create an empty commit. Push master to origin. Then run gh run list --repo kuro1337WStuff/seraph --branch master --limit 5 and report the Windows, Ubuntu, and macOS conclusions. Set real=true only if the push succeeded. summary must include the ctest line and the push result.", #{
    label: "ship",
    capability_mode: "execute",
    output_schema: verdict_schema,
});

if shipped == () || shipped.success == false || shipped.output.real != true {
    await_user("verification", "Push did not succeed.");
}

complete(#{ summary: shipped.output.summary, pushed: true });
