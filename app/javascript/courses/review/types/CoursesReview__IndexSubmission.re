type status = {
  passedAt: option(Js.Date.t),
  feedbackSent: bool,
};

type t = {
  id: string,
  title: string,
  createdAt: Js.Date.t,
  levelId: string,
  userNames: string,
  status: option(status),
  coachIds: array(string),
};

let id = t => t.id;
let title = t => t.title;
let levelId = t => t.levelId;
let userNames = t => t.userNames;
let coachIds = t => t.coachIds;

let failed = t =>
  switch (t.status) {
  | None => false
  | Some(status) =>
    status.passedAt |> OptionUtils.mapWithDefault(_ => false, true)
  };

let pendingReview = t =>
  t.status |> OptionUtils.mapWithDefault(_ => false, true);

let feedbackSent = t =>
  t.status |> OptionUtils.mapWithDefault(status => status.feedbackSent, false);

let createdAtPretty = t => t.createdAt |> DateFns.format("MMMM D, YYYY");

let timeDistance = t =>
  t.createdAt |> DateFns.distanceInWordsToNow(~addSuffix=true);

let sortArray = (sortDirection, submissions) => {
  let sortDescending =
    submissions
    |> ArrayUtils.copyAndSort((x, y) =>
         DateFns.differenceInSeconds(y.createdAt, x.createdAt) |> int_of_float
       );
  switch (sortDirection) {
  | `Descending => sortDescending
  | `Ascending => sortDescending |> Js.Array.reverseInPlace
  };
};

let make = (~id, ~title, ~createdAt, ~levelId, ~userNames, ~status, ~coachIds) => {
  id,
  title,
  createdAt,
  levelId,
  userNames,
  status,
  coachIds,
};

let makeStatus = (~passedAt, ~feedbackSent) => {passedAt, feedbackSent};

let decodeJs = details =>
  details
  |> Js.Array.map(s =>
       switch (s) {
       | Some(submission) =>
         let status =
           submission##evaluatedAt
           |> OptionUtils.map(_ =>
                makeStatus(
                  ~passedAt=
                    submission##passedAt
                    |> OptionUtils.map(DateFns.parseString),
                  ~feedbackSent=submission##feedbackSent,
                )
              );

         [
           make(
             ~id=submission##id,
             ~title=submission##title,
             ~createdAt=submission##createdAt |> DateFns.parseString,
             ~levelId=submission##levelId,
             ~userNames=submission##userNames,
             ~status,
             ~coachIds=submission##coachIds,
           ),
         ];
       | None => []
       }
     );

let replace = (e, l) => l |> Array.map(s => s.id == e.id ? e : s);
