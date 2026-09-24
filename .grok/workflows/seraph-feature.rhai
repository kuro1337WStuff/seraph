let meta = #{
    name: "seraph-feature",
    description: "Implement one Seraph feature, prove it with ctest, and push master",
    phases: [
        #{ title: "Implement", detail: "one feature from the tracker" },
        #{ title: "Test", detail: "add a test and run ctest" },
        #{ title: "Prove", detail: "critic quotes the ctest line" },
        #{ title: "Ship", detail: "commit and push" },
    ],
};

let note_schema = #{
    "type": "object",
    "required": ["summary"],
    "properties": #{
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

let feature = if args == () { () } else { args.feature };
if feature == () {
    pause("verification", "Pass args.feature — an id from docs/SERAPH_FEATURES.md.");
}

let root = "C:\\Users\\Loki\\seraph";
let cmake = "C:\\Program Files\\Microsoft Visual Studio\\2022\\Professional\\Common7\\IDE\\CommonExtensions\\Microsoft\\CMake\\CMake\\bin\\cmake.exe";
let feature_text = feature.to_string();

phase("Implement");
let implemented = agent("Implement the Seraph feature id '" + feature_text + "' in " + root + ". Read docs/SERAPH_FEATURES.md and the Zydis sources before editing. Keep the public library named Zydis. New library code must not call malloc or libc. Caller buffers only. Match the surrounding C style. Update every source list that names src/*.c when you add a library file: CMakeLists.txt, meson.build, msvc project files, and assets/amalgamate.py if it has an explicit list. Do not commit. summary describes the files you changed.", #{
    label: "implement",
    capability_mode: "execute",
    output_schema: note_schema,
});
if implemented == () || implemented.success == false {
    await_user("verification", "Implement step failed for " + feature_text + ".");
}

phase("Test");
let tested = agent("Add or extend a ctest for feature '" + feature_text + "' in " + root + " and run it. cmake: " + cmake + ". Build directory: " + root + "\\build. Configure with -DZYDIS_BUILD_TESTS=ON if needed, build Release, run ctest -C Release --output-on-failure with NO_COLOR=1. Set real=true only if the new test and the existing tests exit 0. summary must quote the ctest summary line. Update docs/SERAPH_FEATURES.md status for this id to done only after that green run. Do not commit.", #{
    label: "test",
    capability_mode: "execute",
    output_schema: verdict_schema,
});
if tested == () || tested.success == false || tested.output.real != true {
    phase("Fix");
    agent("The test step for '" + feature_text + "' in " + root + " was not green. Fix the code and re-run ctest Release until it exits 0. Do not commit.", #{
        label: "fix",
        capability_mode: "execute",
    });
    tested = agent("Re-run ctest -C Release --output-on-failure in " + root + "\\build. Set real=true only if it exits 0. summary must quote the summary line.", #{
        label: "retest",
        capability_mode: "execute",
        output_schema: verdict_schema,
    });
}
if tested == () || tested.success == false || tested.output.real != true || tested.output.summary == () || tested.output.summary == "" {
    await_user("verification", "Feature " + feature_text + " has no green ctest summary.");
}

phase("Prove");
let proved = agent("Independently re-run ctest -C Release --output-on-failure in " + root + "\\build using ctest.exe beside " + cmake + " if needed. Feature under test: '" + feature_text + "'. Set real=true only if you saw exit 0. summary must quote the ctest summary line verbatim. Also name the test that covers this feature.", #{
    label: "prove",
    capability_mode: "execute",
    output_schema: verdict_schema,
});
if proved == () || proved.success == false || proved.output.real != true || proved.output.summary == () || proved.output.summary == "" {
    await_user("verification", "Critic did not quote a green ctest line for " + feature_text + ".");
}

phase("Ship");
let shipped = agent("Repo " + root + " origin https://github.com/kuro1337WStuff/seraph.git branch master. Feature '" + feature_text + "' proved with: " + proved.output.summary + ". Commit the feature with the repo git identity if there are changes. Do not add a Co-Authored-By trailer or any agent attribution. Do not create an empty commit. Push master. Set real=true only if the push succeeded. summary must include the ctest line.", #{
    label: "ship",
    capability_mode: "execute",
    output_schema: verdict_schema,
});
if shipped == () || shipped.success == false || shipped.output.real != true {
    await_user("verification", "Push failed for " + feature_text + ".");
}

complete(#{ feature: feature_text, summary: shipped.output.summary, pushed: true });
